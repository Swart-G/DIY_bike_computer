package com.diybikecomputer.companion.media

import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.ble.BikeProtocol
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class MediaBleCoordinator(
    private val connection: BikeConnectionService,
    private val repository: MediaRepository,
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    init {
        scope.launch {
            repository.state.collect {
                sendState(it)
            }
        }
        scope.launch {
            connection.state.collect {
                if (it == BikeConnectionState.Ready) sendState(repository.state.value)
            }
        }
        scope.launch {
            connection.protocolFrames.collect { frame ->
                if (frame.messageType == BikeProtocol.Message.MEDIA_ACTION &&
                    frame.payload.size == 9
                ) {
                    val buffer = ByteBuffer.wrap(frame.payload).order(ByteOrder.LITTLE_ENDIAN)
                    repository.perform(buffer.get().toInt() and 0xFF, buffer.long)
                }
            }
        }
        scope.launch {
            while (true) {
                delay(1_000)
                val media = repository.state.value
                if (media.available && media.playing) sendState(media)
            }
        }
    }

    private fun sendState(media: MediaSnapshot) {
        if (connection.state.value != BikeConnectionState.Ready) return
        val player = utf8Prefix(media.player, 32)
        val title = utf8Prefix(media.title, 64)
        val artist = utf8Prefix(media.artist, 64)
        val payload = ByteBuffer.allocate(25 + player.size + title.size + artist.size)
            .order(ByteOrder.LITTLE_ENDIAN)
            .put(if (media.available) 1 else 0)
            .put(if (media.playing) 1 else 0)
            .putInt(media.supportedActions)
            .putLong(media.durationMs)
            .putLong(media.currentPositionMs())
            .put(player.size.toByte())
            .put(player)
            .put(title.size.toByte())
            .put(title)
            .put(artist.size.toByte())
            .put(artist)
            .array()
        connection.send(
            BikeProtocol.Message.MEDIA_STATE,
            BikeProtocol.Flag.PRIVILEGED,
            payload,
        )
    }

    private fun utf8Prefix(value: String, maximumBytes: Int): ByteArray {
        val output = ByteArrayOutputStream(maximumBytes)
        var index = 0
        while (index < value.length) {
            val codePoint = value.codePointAt(index)
            val encoded = String(Character.toChars(codePoint)).encodeToByteArray()
            if (output.size() + encoded.size > maximumBytes) break
            output.write(encoded)
            index += Character.charCount(codePoint)
        }
        return output.toByteArray()
    }
}
