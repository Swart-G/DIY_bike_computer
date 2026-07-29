package com.diybikecomputer.companion.ble

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.content.Context
import com.diybikecomputer.companion.device.DeviceRepository
import com.diybikecomputer.companion.device.KnownDevice
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.TimeZone
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
    private var nextSequence = 1
    private var reconnectJob: Job? = null
    private var reconnectAttempt = 0
    private var intentionalDisconnect = false

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
                        sendHello()
                    }
                    GattState.Error -> {
                        mutableState.value = BikeConnectionState.Reconnecting
                        scheduleReconnect()
                    }
                    GattState.Disconnected -> {
                        mutableState.value =
                            if (devices.knownDevice() == null) BikeConnectionState.Unpaired
                            else BikeConnectionState.Reconnecting
                        scheduleReconnect()
                    }
                }
            }
        }
        scope.launch {
            gatt.incoming.collect { value ->
                decoder.feed(value)
                repeat(8) {
                    when (val event = decoder.next()) {
                        is DecodeEvent.Frame -> handleFrame(event.value)
                        DecodeEvent.NeedMoreData -> return@collect
                        else -> mutableState.value = BikeConnectionState.Error
                    }
                }
            }
        }
    }

    fun connect(device: BluetoothDevice) {
        intentionalDisconnect = false
        reconnectJob?.cancel()
        decoder.reset()
        nextSequence = 1
        devices.saveBluetoothAddress(device.address)
        gatt.connect(device)
    }

    fun connectKnown(): Boolean {
        val address = devices.knownBluetoothAddress() ?: return false
        val adapter = appContext.getSystemService(BluetoothManager::class.java).adapter
            ?: return false
        return runCatching {
            val device = adapter.getRemoteDevice(address)
            if (device.bondState != BluetoothDevice.BOND_BONDED) {
                intentionalDisconnect = true
                reconnectJob?.cancel()
                gatt.close()
                devices.forgetApplicationAssociation()
                mutableState.value = BikeConnectionState.Unpaired
            } else {
                connect(device)
            }
            true
        }.getOrDefault(false)
    }

    fun disconnect() {
        intentionalDisconnect = true
        reconnectJob?.cancel()
        gatt.close()
    }
    fun knownDevice(): KnownDevice? = devices.knownDevice()

    fun send(messageType: Int, flags: Int, payload: ByteArray): Boolean {
        return gatt.enqueueFrame(
            BikeProtocolCodec.encode(messageType, flags, nextSequence++, payload),
        )
    }

    private fun sendHello() {
        val associationId = devices.knownDevice()?.associationId ?: 0L
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
        send(BikeProtocol.Message.HELLO, BikeProtocol.Flag.ACK_REQUIRED, payload)
    }

    private fun handleFrame(frame: BikeFrame) {
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
            BikeProtocol.Message.CONFIG_RESULT -> mutableProtocolFrames.tryEmit(frame)
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

    private fun handleProtocolError(frame: BikeFrame) {
        if (frame.payload.size < 6) return
        val buffer = ByteBuffer.wrap(frame.payload).order(ByteOrder.LITTLE_ENDIAN)
        val code = buffer.short.toInt() and 0xFFFF
        val rejectedType = buffer.get().toInt() and 0xFF
        buffer.short
        val detailLength = buffer.get().toInt() and 0xFF
        if (buffer.remaining() != detailLength) return
        val detailBytes = ByteArray(detailLength)
        buffer.get(detailBytes)
        mutableProtocolErrors.tryEmit(
            ProtocolError(code, rejectedType, detailBytes.decodeToString()),
        )
        mutableProtocolFrames.tryEmit(frame)
        if (code == 2 || code == 15) mutableState.value = BikeConnectionState.Error
    }

    private fun scheduleReconnect() {
        if (intentionalDisconnect || devices.knownBluetoothAddress() == null ||
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
            mutableState.value = BikeConnectionState.Error
            return
        }
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        buffer.position(5)
        val deviceId = buffer.long
        val associationId = buffer.long
        buffer.int // capabilities
        val rideFormatCount = buffer.get().toInt() and 0xFF
        if (buffer.remaining() < rideFormatCount + 1 || associationId == 0L) {
            mutableState.value = BikeConnectionState.Error
            return
        }
        buffer.position(buffer.position() + rideFormatCount)
        val nameLength = buffer.get().toInt() and 0xFF
        if (buffer.remaining() < nameLength) {
            mutableState.value = BikeConnectionState.Error
            return
        }
        val nameBytes = ByteArray(nameLength)
        buffer.get(nameBytes)
        devices.save(
            KnownDevice(
                deviceId = deviceId,
                associationId = associationId,
                displayName = nameBytes.decodeToString(),
            ),
        )
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
        mutableTelemetry.value = LiveTelemetry(
            rideState = buffer.get().toInt() and 0xFF,
            motionState = buffer.get().toInt() and 0xFF,
            batteryPercent = buffer.get().toInt() and 0xFF,
            sdState = buffer.get().toInt() and 0xFF,
            speedKmh = buffer.float,
            distanceM = buffer.float,
            averageSpeedKmh = buffer.float,
            maxSpeedKmh = buffer.float,
            movingTimeMs = buffer.long,
            elapsedTimeMs = buffer.long,
            pulseCount = buffer.int.toLong() and 0xFFFF_FFFFL,
            rideId = buffer.int.toLong() and 0xFFFF_FFFFL,
        )
    }

    private fun handleRideEvent(payload: ByteArray) {
        if (payload.size != 13) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        mutableRideEvents.tryEmit(
            RideEvent(
                event = buffer.get().toInt() and 0xFF,
                rideId = buffer.int.toLong() and 0xFFFF_FFFFL,
                elapsedTimeMs = buffer.long,
            ),
        )
    }

    private fun handleDeviceEvent(payload: ByteArray) {
        if (payload.size != 5) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        mutableDeviceEvents.tryEmit(
            DeviceEvent(
                event = buffer.get().toInt() and 0xFF,
                detail = buffer.int.toLong() and 0xFFFF_FFFFL,
            ),
        )
    }
}
