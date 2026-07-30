package com.diybikecomputer.companion.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.Context
import com.diybikecomputer.companion.device.DeviceRepository
import com.diybikecomputer.companion.device.KnownDevice
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.TimeZone
import java.util.concurrent.atomic.AtomicInteger
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay

enum class BikeConnectionState {
    Unpaired,
    Connecting,
    Initializing,
    Ready,
    Reconnecting,
    Disconnected,
    Error,
}

data class LiveTelemetry(
    val rideState: Int = 0,
    val motionState: Int = 0,
    val batteryPercent: Int = 0,
    val sdState: Int = 0,
    val speedKmh: Float = 0f,
    val distanceM: Float = 0f,
    val averageSpeedKmh: Float = 0f,
    val maxSpeedKmh: Float = 0f,
    val movingTimeMs: Long = 0,
    val elapsedTimeMs: Long = 0,
    val pulseCount: Long = 0,
    val rideId: Long = 0,
)

data class RideEvent(
    val event: Int,
    val rideId: Long,
    val elapsedTimeMs: Long,
)

data class DeviceEvent(val event: Int, val detail: Long)
data class ProtocolError(val code: Int, val rejectedType: Int, val detail: String)

class BikeConnectionService(context: Context) {
    private val appContext = context.applicationContext
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val gatt = BikeGattClient(appContext)
    private val devices = DeviceRepository(appContext)
    private val decoder = BikeProtocolDecoder()
    private val mutableState = MutableStateFlow(
        if (devices.knownDevice() == null) BikeConnectionState.Unpaired
        else BikeConnectionState.Reconnecting,
    )
    val state: StateFlow<BikeConnectionState> = mutableState.asStateFlow()
    val knownDevices: StateFlow<List<KnownDevice>> = devices.devices
    private val mutableTelemetry = MutableStateFlow(LiveTelemetry())
    val telemetry: StateFlow<LiveTelemetry> = mutableTelemetry.asStateFlow()
    private val mutableRideEvents = MutableSharedFlow<RideEvent>(extraBufferCapacity = 8)
    val rideEvents: SharedFlow<RideEvent> = mutableRideEvents
    private val mutableDeviceEvents = MutableSharedFlow<DeviceEvent>(extraBufferCapacity = 8)
    val deviceEvents: SharedFlow<DeviceEvent> = mutableDeviceEvents
    private val mutableProtocolFrames = MutableSharedFlow<BikeFrame>(extraBufferCapacity = 16)
    val protocolFrames: SharedFlow<BikeFrame> = mutableProtocolFrames
    private val mutableProtocolErrors = MutableSharedFlow<ProtocolError>(extraBufferCapacity = 8)
    val protocolErrors: SharedFlow<ProtocolError> = mutableProtocolErrors
    private val nextSequence = AtomicInteger(1)
    private var reconnectJob: Job? = null
    private var initializationJob: Job? = null
    private var reconnectAttempt = 0
    @Volatile
    private var intentionalDisconnect = false
    @Volatile
    private var activeBluetoothAddress: String? = devices.knownDevice()?.bluetoothAddress

    init {
        scope.launch {
            gatt.state.collect { gattState ->
                when (gattState) {
                    GattState.Connecting -> mutableState.value = BikeConnectionState.Connecting
                    GattState.Discovering, GattState.Subscribing ->
                        mutableState.value = BikeConnectionState.Initializing
                    GattState.Ready -> {
                        reconnectJob?.cancel()
                        reconnectAttempt = 0
                        mutableState.value = BikeConnectionState.Initializing
                        if (!sendHello()) {
                            gatt.close()
                        } else {
                            initializationJob?.cancel()
                            initializationJob = scope.launch {
                                delay(INITIALIZATION_TIMEOUT_MS)
                                if (mutableState.value == BikeConnectionState.Initializing) {
                                    gatt.close()
                                }
                            }
                        }
                    }
                    GattState.Error -> {
                        initializationJob?.cancel()
                        mutableState.value = BikeConnectionState.Reconnecting
                        scheduleReconnect()
                    }
                    GattState.Disconnected -> {
                        initializationJob?.cancel()
                        mutableState.value =
                            if (devices.knownDevices().isEmpty()) BikeConnectionState.Unpaired
                            else if (intentionalDisconnect) BikeConnectionState.Disconnected
                            else BikeConnectionState.Reconnecting
                        scheduleReconnect()
                    }
                }
            }
        }
        scope.launch {
            gatt.incoming.collect { value ->
                val frames = decodeIncoming(value)
                if (frames == null) gatt.close() else frames.forEach { handleFrame(it) }
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice, systemAssociationId: Int? = null) {
        intentionalDisconnect = false
        reconnectJob?.cancel()
        initializationJob?.cancel()
        synchronized(decoder) { decoder.reset() }
        nextSequence.set(1)
        activeBluetoothAddress = device.address
        devices.rememberEndpoint(
            bluetoothAddress = device.address,
            displayName = runCatching { device.name }.getOrNull(),
            systemAssociationId = systemAssociationId,
        )
        gatt.connect(device)
        initializationJob = scope.launch {
            delay(CONNECTION_TIMEOUT_MS)
            if (mutableState.value == BikeConnectionState.Connecting ||
                mutableState.value == BikeConnectionState.Initializing
            ) {
                gatt.close()
            }
        }
    }

    @SuppressLint("MissingPermission")
    fun connectKnown(bluetoothAddress: String? = null): Boolean {
        val target = bluetoothAddress?.let(devices::find) ?: devices.knownDevice()
            ?: return false
        val address = target.bluetoothAddress
        devices.select(address)
        activeBluetoothAddress = address
        val adapter = appContext.getSystemService(BluetoothManager::class.java).adapter
            ?: return false
        return runCatching {
            val device = adapter.getRemoteDevice(address)
            if (device.bondState != BluetoothDevice.BOND_BONDED) {
                intentionalDisconnect = true
                reconnectJob?.cancel()
                initializationJob?.cancel()
                gatt.close()
                devices.forget(address)
                mutableState.value =
                    if (devices.knownDevices().isEmpty()) BikeConnectionState.Unpaired
                    else BikeConnectionState.Disconnected
            } else {
                connect(device, target.systemAssociationId)
            }
            true
        }.getOrDefault(false)
    }

    fun disconnect() {
        intentionalDisconnect = true
        reconnectJob?.cancel()
        initializationJob?.cancel()
        gatt.close()
    }

    fun forgetDevice(bluetoothAddress: String): KnownDevice? {
        val isActive = activeBluetoothAddress.equals(bluetoothAddress, ignoreCase = true)
        if (isActive) {
            intentionalDisconnect = true
            reconnectJob?.cancel()
            initializationJob?.cancel()
            gatt.close()
            activeBluetoothAddress = null
            mutableTelemetry.value = LiveTelemetry()
        }
        val removed = devices.forget(bluetoothAddress)
        if (isActive || devices.knownDevices().isEmpty()) {
            mutableState.value =
                if (devices.knownDevices().isEmpty()) BikeConnectionState.Unpaired
                else BikeConnectionState.Disconnected
        }
        return removed
    }

    fun knownDevice(): KnownDevice? = devices.knownDevice()

    fun send(messageType: Int, flags: Int, payload: ByteArray): Boolean {
        val sequence = nextSequence.getAndUpdate { current ->
            if (current >= 0xFFFF) 1 else current + 1
        }
        return gatt.enqueueFrame(
            BikeProtocolCodec.encode(messageType, flags, sequence, payload),
        )
    }

    private fun decodeIncoming(value: ByteArray): List<BikeFrame>? =
        synchronized(decoder) {
            decoder.feed(value)
            val frames = ArrayList<BikeFrame>()
            repeat(MAX_DECODE_EVENTS_PER_NOTIFICATION) {
                when (val event = decoder.next()) {
                    is DecodeEvent.Frame -> frames += event.value
                    DecodeEvent.NeedMoreData -> return@synchronized frames
                    else -> {
                        decoder.reset()
                        return@synchronized null
                    }
                }
            }
            decoder.reset()
            null
        }

    private fun sendHello(): Boolean {
        val associationId = activeBluetoothAddress?.let(devices::find)?.associationId ?: 0L
        val name = "Bike Computer Android".encodeToByteArray()
        val payload = ByteBuffer.allocate(3 + 1 + 1 + 8 + 4 + 1 + name.size)
            .order(ByteOrder.LITTLE_ENDIAN)
            .put(2)
            .put(0)
            .put(0)
            .put(1)
            .put(BikeProtocol.VERSION.toByte())
            .putLong(associationId)
            .putInt(BikeProtocol.CAPABILITIES)
            .put(name.size.toByte())
            .put(name)
            .array()
        return send(BikeProtocol.Message.HELLO, BikeProtocol.Flag.ACK_REQUIRED, payload)
    }

    private suspend fun handleFrame(frame: BikeFrame) {
        when (frame.messageType) {
            BikeProtocol.Message.HELLO_ACK -> handleHelloAck(frame.payload)
            BikeProtocol.Message.LIVE_TELEMETRY -> handleTelemetry(frame.payload)
            BikeProtocol.Message.RIDE_EVENT -> handleRideEvent(frame.payload)
            BikeProtocol.Message.DEVICE_EVENT -> handleDeviceEvent(frame.payload)
            BikeProtocol.Message.TIME_SYNC -> Unit
            BikeProtocol.Message.RIDE_MANIFEST,
            BikeProtocol.Message.RIDE_LIST_END,
            BikeProtocol.Message.FILE_BEGIN,
            BikeProtocol.Message.FILE_CHUNK,
            BikeProtocol.Message.FILE_END,
            BikeProtocol.Message.MEDIA_ACTION,
            BikeProtocol.Message.CONFIG_VALUE,
            BikeProtocol.Message.CONFIG_RESULT -> mutableProtocolFrames.emit(frame)
            BikeProtocol.Message.PING -> {
                gatt.enqueueFrame(
                    BikeProtocolCodec.encode(
                        BikeProtocol.Message.PONG,
                        BikeProtocol.Flag.RESPONSE,
                        frame.sequence,
                        frame.payload,
                    ),
                )
            }
            BikeProtocol.Message.ERROR -> handleProtocolError(frame)
        }
    }

    private suspend fun handleProtocolError(frame: BikeFrame) {
        if (frame.payload.size < 6) return
        val buffer = ByteBuffer.wrap(frame.payload).order(ByteOrder.LITTLE_ENDIAN)
        val code = buffer.short.toInt() and 0xFFFF
        val rejectedType = buffer.get().toInt() and 0xFF
        buffer.short
        val detailLength = buffer.get().toInt() and 0xFF
        if (buffer.remaining() != detailLength) return
        val detailBytes = ByteArray(detailLength)
        buffer.get(detailBytes)
        mutableProtocolErrors.emit(
            ProtocolError(code, rejectedType, detailBytes.decodeToString()),
        )
        mutableProtocolFrames.emit(frame)
        if (code == 2 || code == 15) gatt.close()
    }

    private fun scheduleReconnect() {
        if (intentionalDisconnect || devices.knownDevice() == null ||
            reconnectJob?.isActive == true
        ) {
            return
        }
        reconnectJob = scope.launch {
            val delays = longArrayOf(1_000, 2_000, 4_000, 8_000, 15_000, 30_000)
            delay(delays[reconnectAttempt.coerceAtMost(delays.lastIndex)])
            reconnectAttempt = (reconnectAttempt + 1).coerceAtMost(delays.lastIndex)
            // connect() cancels the outstanding retry. Clear this job first so an
            // immediate connect failure can schedule the next bounded attempt.
            reconnectJob = null
            if (!connectKnown()) scheduleReconnect()
        }
    }

    private fun handleHelloAck(payload: ByteArray) {
        if (payload.size < 26 || (payload[0].toInt() and 0xFF) != BikeProtocol.VERSION) {
            gatt.close()
            return
        }
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        buffer.position(5)
        val deviceId = buffer.long
        val associationId = buffer.long
        buffer.int // capabilities
        val rideFormatCount = buffer.get().toInt() and 0xFF
        if (buffer.remaining() < rideFormatCount + 1 ||
            deviceId == 0L || associationId == 0L
        ) {
            gatt.close()
            return
        }
        buffer.position(buffer.position() + rideFormatCount)
        val nameLength = buffer.get().toInt() and 0xFF
        if (buffer.remaining() < nameLength) {
            gatt.close()
            return
        }
        val nameBytes = ByteArray(nameLength)
        buffer.get(nameBytes)
        val bluetoothAddress = activeBluetoothAddress ?: return run {
            gatt.close()
        }
        val endpoint = devices.find(bluetoothAddress)
        devices.save(
            KnownDevice(
                deviceId = deviceId,
                associationId = associationId,
                displayName = nameBytes.decodeToString()
                    .trim()
                    .take(MAX_DEVICE_NAME_LENGTH)
                    .ifBlank { "DIY Bike Computer" },
                bluetoothAddress = bluetoothAddress,
                systemAssociationId = endpoint?.systemAssociationId,
            ),
        )
        initializationJob?.cancel()
        mutableState.value = BikeConnectionState.Ready
        sendTimeSync()
    }

    private fun sendTimeSync() {
        val now = System.currentTimeMillis()
        val timeZone = TimeZone.getDefault()
        val zone = timeZone.id.encodeToByteArray().let {
            if (it.size <= 47) it else it.copyOf(47)
        }
        val payload = ByteBuffer.allocate(8 + 4 + 1 + zone.size)
            .order(ByteOrder.LITTLE_ENDIAN)
            .putLong(now)
            .putInt(timeZone.getOffset(now) / 1000)
            .put(zone.size.toByte())
            .put(zone)
            .array()
        send(BikeProtocol.Message.TIME_SYNC, BikeProtocol.Flag.ACK_REQUIRED, payload)
    }

    private fun handleTelemetry(payload: ByteArray) {
        if (payload.size != 44) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val rideState = buffer.get().toInt() and 0xFF
        val motionState = buffer.get().toInt() and 0xFF
        val batteryPercent = buffer.get().toInt() and 0xFF
        val sdState = buffer.get().toInt() and 0xFF
        val speedKmh = buffer.float
        val distanceM = buffer.float
        val averageSpeedKmh = buffer.float
        val maxSpeedKmh = buffer.float
        val movingTimeMs = buffer.long
        val elapsedTimeMs = buffer.long
        val pulseCount = buffer.int.toLong() and 0xFFFF_FFFFL
        val rideId = buffer.int.toLong() and 0xFFFF_FFFFL
        if (rideState !in 0..3 || motionState !in 0..1 || batteryPercent > 100 ||
            sdState !in 0..3 || !speedKmh.isFinite() || speedKmh < 0f ||
            !distanceM.isFinite() || distanceM < 0f ||
            !averageSpeedKmh.isFinite() || averageSpeedKmh < 0f ||
            !maxSpeedKmh.isFinite() || maxSpeedKmh < 0f ||
            movingTimeMs < 0 || elapsedTimeMs < 0 || movingTimeMs > elapsedTimeMs
        ) {
            return
        }
        mutableTelemetry.value = LiveTelemetry(
            rideState = rideState,
            motionState = motionState,
            batteryPercent = batteryPercent,
            sdState = sdState,
            speedKmh = speedKmh,
            distanceM = distanceM,
            averageSpeedKmh = averageSpeedKmh,
            maxSpeedKmh = maxSpeedKmh,
            movingTimeMs = movingTimeMs,
            elapsedTimeMs = elapsedTimeMs,
            pulseCount = pulseCount,
            rideId = rideId,
        )
    }

    private suspend fun handleRideEvent(payload: ByteArray) {
        if (payload.size != 13) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val event = buffer.get().toInt() and 0xFF
        val rideId = buffer.int.toLong() and 0xFFFF_FFFFL
        val elapsedTimeMs = buffer.long
        if (elapsedTimeMs < 0) return
        mutableRideEvents.emit(
            RideEvent(
                event = event,
                rideId = rideId,
                elapsedTimeMs = elapsedTimeMs,
            ),
        )
    }

    private suspend fun handleDeviceEvent(payload: ByteArray) {
        if (payload.size != 5) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        mutableDeviceEvents.emit(
            DeviceEvent(
                event = buffer.get().toInt() and 0xFF,
                detail = buffer.int.toLong() and 0xFFFF_FFFFL,
            ),
        )
    }

    private companion object {
        const val MAX_DECODE_EVENTS_PER_NOTIFICATION = 64
        const val MAX_DEVICE_NAME_LENGTH = 64
        const val CONNECTION_TIMEOUT_MS = 15_000L
        const val INITIALIZATION_TIMEOUT_MS = 10_000L
    }
}
