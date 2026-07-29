package com.diybikecomputer.companion.rides

import android.content.Context
import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.ble.BikeFrame
import com.diybikecomputer.companion.ble.BikeProtocol
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

enum class RideSyncState {
    Idle,
    Listing,
    Downloading,
    Verifying,
    Complete,
    Error,
}

data class RideSyncProgress(
    val state: RideSyncState = RideSyncState.Idle,
    val rideId: String? = null,
    val fileId: Int = 0,
    val downloadedBytes: Long = 0,
    val totalBytes: Long = 0,
    val error: String? = null,
)

private data class ManifestFile(val id: Int, val size: Long, val crc32: Long)

private data class Manifest(
    val numericRideId: Long,
    val rideKey: String,
    val revision: Long,
    val files: List<ManifestFile>,
)

class RideSyncManager(
    context: Context,
    private val connection: BikeConnectionService,
    private val database: RideDatabase,
) {
    private val scope =
        CoroutineScope(SupervisorJob() + Dispatchers.IO.limitedParallelism(1))
    private val importer = RideImporter(database)
    private val root = File(context.filesDir, "rides")
    private val manifests = LinkedHashMap<String, Manifest>()
    private val pending = ArrayDeque<Pair<Manifest, ManifestFile>>()
    private val mutableProgress = MutableStateFlow(RideSyncProgress())
    val progress: StateFlow<RideSyncProgress> = mutableProgress.asStateFlow()
    private var listRequested = false
    private var current: Pair<Manifest, ManifestFile>? = null
    private var output: RandomAccessFile? = null
    private var currentOffset = 0L
    private var currentChunkSequence = 0
    private var lastRideState = -1

    init {
        root.mkdirs()
        scope.launch {
            connection.state.collect { state ->
                if (state != BikeConnectionState.Ready) {
                    closeOutput()
                    listRequested = false
                    current = null
                    pending.clear()
                } else {
                    requestListIfAllowed()
                }
            }
        }
        scope.launch {
            connection.telemetry.collect { telemetry ->
                val becameInactive =
                    (lastRideState == 1 || lastRideState == 2) &&
                        (telemetry.rideState == 0 || telemetry.rideState == 3)
                lastRideState = telemetry.rideState
                if (becameInactive) listRequested = false
                requestListIfAllowed()
            }
        }
        scope.launch {
            connection.protocolFrames.collect(::handleFrame)
        }
    }

    private fun requestListIfAllowed() {
        val telemetry = connection.telemetry.value
        if (listRequested || connection.state.value != BikeConnectionState.Ready ||
            telemetry.sdState != 1 || (telemetry.rideState != 0 && telemetry.rideState != 3)
        ) {
            return
        }
        if (connection.send(
                BikeProtocol.Message.RIDE_LIST_REQUEST,
                BikeProtocol.Flag.ACK_REQUIRED or BikeProtocol.Flag.PRIVILEGED,
                ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(0).array(),
            )
        ) {
            listRequested = true
            manifests.clear()
            pending.clear()
            mutableProgress.value = RideSyncProgress(state = RideSyncState.Listing)
        }
    }

    private suspend fun handleFrame(frame: BikeFrame) {
        when (frame.messageType) {
            BikeProtocol.Message.RIDE_MANIFEST -> handleManifest(frame.payload)
            BikeProtocol.Message.RIDE_LIST_END -> finishListing()
            BikeProtocol.Message.FILE_BEGIN -> handleFileBegin(frame.payload)
            BikeProtocol.Message.FILE_CHUNK -> handleFileChunk(frame.payload)
            BikeProtocol.Message.FILE_END -> handleFileEnd(frame.payload)
        }
    }

    private suspend fun handleManifest(payload: ByteArray) {
        if (payload.size < 43) return fail("Short ride manifest")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val numericRideId = buffer.int.toLong() and 0xFFFF_FFFFL
        val formatVersion = buffer.get().toInt() and 0xFF
        val finished = buffer.get().toInt() != 0
        val startedAt = buffer.long.takeIf { it >= 0 }
        val finishedAt = buffer.long.takeIf { it >= 0 }
        val distanceM = buffer.float.toDouble()
        val durationMs = buffer.long
        val revision = buffer.int.toLong() and 0xFFFF_FFFFL
        buffer.int // total size
        val fileCount = buffer.get().toInt() and 0xFF
        if (fileCount > 4 || buffer.remaining() != fileCount * 9) {
            return fail("Invalid ride manifest file list")
        }
        val known = connection.knownDevice() ?: return fail("Device association missing")
        val rideKey = "${java.lang.Long.toUnsignedString(known.deviceId)}:$numericRideId"
        val files = buildList {
            repeat(fileCount) {
                add(
                    ManifestFile(
                        id = buffer.get().toInt() and 0xFF,
                        size = buffer.int.toLong() and 0xFFFF_FFFFL,
                        crc32 = buffer.int.toLong() and 0xFFFF_FFFFL,
                    ),
                )
            }
        }
        database.deviceDao().upsert(
            DeviceEntity(
                deviceId = known.deviceId,
                associationId = known.associationId,
                displayName = known.displayName,
                lastSeenUtcMs = System.currentTimeMillis(),
                protocolVersion = BikeProtocol.VERSION,
            ),
        )
        val previous = database.rideDao().getRide(rideKey)
        database.rideDao().upsertRide(
            RideEntity(
                rideId = rideKey,
                deviceId = known.deviceId,
                formatVersion = formatVersion,
                startedAtUtcMs = startedAt,
                finishedAtUtcMs = finishedAt,
                finished = finished,
                distanceM = distanceM,
                movingTimeMs = previous?.movingTimeMs ?: 0,
                elapsedTimeMs = durationMs,
                averageSpeedKmh = previous?.averageSpeedKmh ?: 0.0,
                maxSpeedKmh = previous?.maxSpeedKmh ?: 0.0,
                syncRevision = revision,
                synced = previous?.synced == true && previous.syncRevision == revision,
            ),
        )
        val manifest = Manifest(numericRideId, rideKey, revision, files)
        manifests[rideKey] = manifest
        prepareFileRows(manifest)
    }

    private suspend fun prepareFileRows(manifest: Manifest) {
        val rideDirectory = File(root, safeDirectoryName(manifest.rideKey)).apply { mkdirs() }
        for (file in manifest.files) {
            val existing = database.rideDao().getFile(manifest.rideKey, file.id)
            val part = File(rideDirectory, "${fileName(file.id)}.part")
            val final = File(rideDirectory, fileName(file.id))
            val reusable = existing?.expectedSize == file.size &&
                existing.expectedCrc32 == file.crc32
            if (!reusable) {
                part.delete()
                final.delete()
            }
            val verified = reusable && existing.verified && final.isFile
            val downloaded = if (verified) file.size else part.length().coerceAtMost(file.size)
            if (part.length() > file.size) part.delete()
            database.rideDao().upsertFile(
                RideFileEntity(
                    rideId = manifest.rideKey,
                    fileId = file.id,
                    expectedSize = file.size,
                    expectedCrc32 = file.crc32,
                    downloadedBytes = downloaded,
                    partialPath = part.absolutePath,
                    verifiedPath = final.absolutePath.takeIf { verified },
                    verified = verified,
                ),
            )
            if (!verified) pending += manifest to file
        }
    }

    private suspend fun finishListing() {
        requestNextFile()
    }

    private suspend fun requestNextFile() {
        closeOutput()
        current = pending.removeFirstOrNull()
        val next = current
        if (next == null) {
            manifests.values.forEach { manifest ->
                val files = database.rideDao().getFiles(manifest.rideKey)
                if (files.size == manifest.files.size && files.all { it.verified }) {
                    importer.importVerifiedFiles(manifest.rideKey, files)
                    database.rideDao().setSynced(manifest.rideKey, true)
                }
            }
            mutableProgress.value = RideSyncProgress(state = RideSyncState.Complete)
            return
        }
        val row = database.rideDao().getFile(next.first.rideKey, next.second.id)
            ?: return fail("Missing local file row")
        val part = File(row.partialPath)
        currentOffset = part.length().coerceAtMost(row.expectedSize)
        val payload = ByteBuffer.allocate(13).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(next.first.numericRideId.toInt())
            .put(next.second.id.toByte())
            .putInt(currentOffset.toInt())
            .putInt(next.second.crc32.toInt())
            .array()
        if (!connection.send(
                BikeProtocol.Message.RIDE_DOWNLOAD_REQUEST,
                BikeProtocol.Flag.ACK_REQUIRED or BikeProtocol.Flag.PRIVILEGED,
                payload,
            )
        ) {
            fail("Cannot queue download request")
        } else {
            mutableProgress.value = RideSyncProgress(
                state = RideSyncState.Downloading,
                rideId = next.first.rideKey,
                fileId = next.second.id,
                downloadedBytes = currentOffset,
                totalBytes = next.second.size,
            )
        }
    }

    private suspend fun handleFileBegin(payload: ByteArray) {
        val expected = current ?: return fail("Unexpected FILE_BEGIN")
        if (payload.size != 19) return fail("Invalid FILE_BEGIN")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val rideId = buffer.int.toLong() and 0xFFFF_FFFFL
        val fileId = buffer.get().toInt() and 0xFF
        val totalSize = buffer.int.toLong() and 0xFFFF_FFFFL
        val acceptedOffset = buffer.int.toLong() and 0xFFFF_FFFFL
        val crc = buffer.int.toLong() and 0xFFFF_FFFFL
        val chunkMaximum = buffer.short.toInt() and 0xFFFF
        if (rideId != expected.first.numericRideId || fileId != expected.second.id ||
            totalSize != expected.second.size || crc != expected.second.crc32 ||
            acceptedOffset > totalSize || chunkMaximum <= 0
        ) {
            return fail("FILE_BEGIN does not match manifest")
        }
        val row = database.rideDao().getFile(expected.first.rideKey, fileId)
            ?: return fail("Missing partial file")
        output = RandomAccessFile(row.partialPath, "rw").also {
            it.setLength(acceptedOffset)
            it.seek(acceptedOffset)
        }
        currentOffset = acceptedOffset
        currentChunkSequence = 0
    }

    private suspend fun handleFileChunk(payload: ByteArray) {
        val expected = current ?: return fail("Unexpected FILE_CHUNK")
        val file = output ?: return fail("FILE_CHUNK before FILE_BEGIN")
        if (payload.size < 13) return fail("Short FILE_CHUNK")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val rideId = buffer.int.toLong() and 0xFFFF_FFFFL
        val fileId = buffer.get().toInt() and 0xFF
        val offset = buffer.int.toLong() and 0xFFFF_FFFFL
        val sequence = buffer.short.toInt() and 0xFFFF
        val length = buffer.short.toInt() and 0xFFFF
        if (rideId != expected.first.numericRideId || fileId != expected.second.id ||
            offset != currentOffset || sequence != currentChunkSequence ||
            length != buffer.remaining() || currentOffset + length > expected.second.size
        ) {
            sendAck(expected, currentOffset, sequence, 2)
            return fail("Invalid FILE_CHUNK sequence")
        }
        val data = ByteArray(length)
        buffer.get(data)
        file.write(data)
        file.fd.sync()
        currentOffset += length
        currentChunkSequence = (currentChunkSequence + 1) and 0xFFFF
        val row = database.rideDao().getFile(expected.first.rideKey, fileId)
            ?: return fail("Missing progress row")
        database.rideDao().upsertFile(row.copy(downloadedBytes = currentOffset))
        sendAck(expected, currentOffset, sequence, 0)
        mutableProgress.value = mutableProgress.value.copy(downloadedBytes = currentOffset)
    }

    private fun sendAck(
        expected: Pair<Manifest, ManifestFile>,
        nextOffset: Long,
        sequence: Int,
        status: Int,
    ) {
        val payload = ByteBuffer.allocate(12).order(ByteOrder.LITTLE_ENDIAN)
            .putInt(expected.first.numericRideId.toInt())
            .put(expected.second.id.toByte())
            .putInt(nextOffset.toInt())
            .putShort(sequence.toShort())
            .put(status.toByte())
            .array()
        connection.send(
            BikeProtocol.Message.FILE_ACK,
            BikeProtocol.Flag.PRIVILEGED,
            payload,
        )
    }

    private suspend fun handleFileEnd(payload: ByteArray) {
        val expected = current ?: return fail("Unexpected FILE_END")
        if (payload.size != 13) return fail("Invalid FILE_END")
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val rideId = buffer.int.toLong() and 0xFFFF_FFFFL
        val fileId = buffer.get().toInt() and 0xFF
        val totalSize = buffer.int.toLong() and 0xFFFF_FFFFL
        val crc = buffer.int.toLong() and 0xFFFF_FFFFL
        closeOutput()
        val row = database.rideDao().getFile(expected.first.rideKey, fileId)
            ?: return fail("Missing completed file")
        val part = File(row.partialPath)
        mutableProgress.value = mutableProgress.value.copy(state = RideSyncState.Verifying)
        if (rideId != expected.first.numericRideId || fileId != expected.second.id ||
            totalSize != expected.second.size || crc != expected.second.crc32 ||
            part.length() != totalSize || crc32(part) != crc
        ) {
            part.delete()
            database.rideDao().upsertFile(
                row.copy(downloadedBytes = 0, verifiedPath = null, verified = false),
            )
            pending.addFirst(expected)
            return requestNextFile()
        }
        val final = File(part.parentFile, fileName(fileId))
        final.delete()
        if (!part.renameTo(final)) return fail("Cannot finalize downloaded file")
        database.rideDao().upsertFile(
            row.copy(
                downloadedBytes = totalSize,
                verifiedPath = final.absolutePath,
                verified = true,
            ),
        )
        requestNextFile()
    }

    private fun closeOutput() {
        runCatching { output?.fd?.sync() }
        runCatching { output?.close() }
        output = null
    }

    private fun crc32(file: File): Long {
        val crc = CRC32()
        file.inputStream().buffered().use { input ->
            val buffer = ByteArray(8192)
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                crc.update(buffer, 0, count)
            }
        }
        return crc.value
    }

    private fun safeDirectoryName(rideId: String): String =
        rideId.replace(Regex("[^A-Za-z0-9._-]"), "_")

    private fun fileName(fileId: Int): String = when (fileId) {
        1 -> "meta.json"
        2 -> "samples.csv"
        3 -> "events.csv"
        4 -> "summary.json"
        else -> "unknown_$fileId"
    }

    private fun fail(message: String) {
        closeOutput()
        mutableProgress.value = mutableProgress.value.copy(
            state = RideSyncState.Error,
            error = message,
        )
    }
}
