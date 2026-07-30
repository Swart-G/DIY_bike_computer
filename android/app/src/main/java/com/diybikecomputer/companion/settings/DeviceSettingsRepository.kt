package com.diybikecomputer.companion.settings

import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.ble.BikeProtocol
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class DeviceSettings(
    val wheelCircumferenceM: Float = 2.194f,
    val stopThresholdKmh: Float = 3f,
    val autoPauseEnabled: Boolean = true,
    val autoPauseDelayMs: Long = 5_000,
    val logIntervalMs: Long = 1_000,
    val graphWindowSeconds: Long = 60,
    val speedLedEnabled: Boolean = true,
    val speedLedTolerance2sKmh: Float = 0.5f,
    val speedLedTolerance5sKmh: Float = 0.5f,
    val speedLedTolerance10sKmh: Float = 0.5f,
    val speedLedBrightnessPercent: Long = 20,
    val loadedKeys: Set<Int> = emptySet(),
    val lastResult: String? = null,
)

class DeviceSettingsRepository(private val connection: BikeConnectionService) {
    private object Key {
        const val WHEEL = 1
        const val STOP_THRESHOLD = 2
        const val AUTO_PAUSE = 3
        const val AUTO_PAUSE_DELAY = 4
        const val LOG_INTERVAL = 5
        const val GRAPH_WINDOW = 6
        const val SPEED_LED_ENABLED = 7
        const val SPEED_LED_TOLERANCE_2S = 8
        const val SPEED_LED_TOLERANCE_5S = 9
        const val SPEED_LED_TOLERANCE_10S = 10
        const val SPEED_LED_BRIGHTNESS = 11
        val all = listOf(
            WHEEL,
            STOP_THRESHOLD,
            AUTO_PAUSE,
            AUTO_PAUSE_DELAY,
            LOG_INTERVAL,
            GRAPH_WINDOW,
            SPEED_LED_ENABLED,
            SPEED_LED_TOLERANCE_2S,
            SPEED_LED_TOLERANCE_5S,
            SPEED_LED_TOLERANCE_10S,
            SPEED_LED_BRIGHTNESS,
        )
    }

    private object Type {
        const val BOOLEAN = 1
        const val U32 = 2
        const val FLOAT32 = 3
    }

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val mutableState = MutableStateFlow(DeviceSettings())
    val state: StateFlow<DeviceSettings> = mutableState.asStateFlow()

    init {
        scope.launch {
            connection.state.collect { state ->
                if (state == BikeConnectionState.Ready) refresh()
            }
        }
        scope.launch {
            connection.protocolFrames.collect { frame ->
                when (frame.messageType) {
                    BikeProtocol.Message.CONFIG_VALUE -> handleValue(frame.payload)
                    BikeProtocol.Message.CONFIG_RESULT -> handleResult(frame.payload)
                }
            }
        }
    }

    fun refresh() {
        if (connection.state.value != BikeConnectionState.Ready) return
        Key.all.forEach { key ->
            val payload = ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN)
                .putShort(key.toShort())
                .array()
            connection.send(
                BikeProtocol.Message.CONFIG_GET,
                BikeProtocol.Flag.ACK_REQUIRED or BikeProtocol.Flag.PRIVILEGED,
                payload,
            )
        }
    }

    fun setAutoPause(enabled: Boolean) =
        set(Key.AUTO_PAUSE, Type.BOOLEAN, if (enabled) 1 else 0)

    fun setAutoPauseDelay(delayMs: Long) {
        if (delayMs in 1_000..60_000) set(Key.AUTO_PAUSE_DELAY, Type.U32, delayMs.toInt())
    }

    fun setWheelCircumference(valueM: Float) {
        if (valueM in 0.5f..3.5f) set(Key.WHEEL, Type.FLOAT32, valueM.toRawBits())
    }

    fun setStopThreshold(valueKmh: Float) {
        if (valueKmh in 0.5f..15f) {
            set(Key.STOP_THRESHOLD, Type.FLOAT32, valueKmh.toRawBits())
        }
    }

    fun setLogInterval(intervalMs: Long) {
        if (intervalMs in 250..10_000) set(Key.LOG_INTERVAL, Type.U32, intervalMs.toInt())
    }

    fun setGraphWindow(windowSeconds: Long) {
        if (windowSeconds in 10..300) set(Key.GRAPH_WINDOW, Type.U32, windowSeconds.toInt())
    }

    fun setSpeedLedEnabled(enabled: Boolean) =
        set(Key.SPEED_LED_ENABLED, Type.BOOLEAN, if (enabled) 1 else 0)

    fun setSpeedLedTolerance2s(valueKmh: Float) =
        setFloatInRange(Key.SPEED_LED_TOLERANCE_2S, valueKmh, 0.1f..5f)

    fun setSpeedLedTolerance5s(valueKmh: Float) =
        setFloatInRange(Key.SPEED_LED_TOLERANCE_5S, valueKmh, 0.1f..5f)

    fun setSpeedLedTolerance10s(valueKmh: Float) =
        setFloatInRange(Key.SPEED_LED_TOLERANCE_10S, valueKmh, 0.1f..5f)

    fun setSpeedLedBrightness(percent: Long) {
        if (percent in 5..100) set(Key.SPEED_LED_BRIGHTNESS, Type.U32, percent.toInt())
    }

    private fun setFloatInRange(key: Int, value: Float, range: ClosedFloatingPointRange<Float>) {
        if (value in range) set(key, Type.FLOAT32, value.toRawBits())
    }

    private fun set(key: Int, type: Int, rawValue: Int) {
        if (connection.state.value != BikeConnectionState.Ready) return
        val payload = ByteBuffer.allocate(7).order(ByteOrder.LITTLE_ENDIAN)
            .putShort(key.toShort())
            .put(type.toByte())
            .putInt(rawValue)
            .array()
        if (!connection.send(
            BikeProtocol.Message.CONFIG_SET,
            BikeProtocol.Flag.ACK_REQUIRED or BikeProtocol.Flag.PRIVILEGED,
            payload,
        )) {
            mutableState.value = mutableState.value.copy(
                lastResult = "Setting $key could not be queued",
            )
        }
    }

    private fun handleValue(payload: ByteArray) {
        if (payload.size != 7) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val key = buffer.short.toInt() and 0xFFFF
        val type = buffer.get().toInt() and 0xFF
        val raw = buffer.int
        val previous = mutableState.value
        if ((key == Key.WHEEL || key == Key.STOP_THRESHOLD ||
                key == Key.SPEED_LED_TOLERANCE_2S ||
                key == Key.SPEED_LED_TOLERANCE_5S ||
                key == Key.SPEED_LED_TOLERANCE_10S) && type != Type.FLOAT32
        ) {
            return
        }
        if ((key == Key.AUTO_PAUSE || key == Key.SPEED_LED_ENABLED) &&
            type != Type.BOOLEAN
        ) {
            return
        }
        if ((key == Key.AUTO_PAUSE_DELAY || key == Key.LOG_INTERVAL ||
                key == Key.GRAPH_WINDOW || key == Key.SPEED_LED_BRIGHTNESS) &&
            type != Type.U32
        ) {
            return
        }
        val floatValue = Float.fromBits(raw)
        val unsignedValue = raw.toLong() and 0xFFFF_FFFFL
        val valid = when (key) {
            Key.WHEEL -> floatValue.isFinite() && floatValue in 0.5f..3.5f
            Key.STOP_THRESHOLD -> floatValue.isFinite() && floatValue in 0.5f..15f
            Key.AUTO_PAUSE, Key.SPEED_LED_ENABLED -> raw == 0 || raw == 1
            Key.AUTO_PAUSE_DELAY -> unsignedValue in 1_000L..60_000L
            Key.LOG_INTERVAL -> unsignedValue in 250L..10_000L
            Key.GRAPH_WINDOW -> unsignedValue in 10L..300L
            Key.SPEED_LED_TOLERANCE_2S,
            Key.SPEED_LED_TOLERANCE_5S,
            Key.SPEED_LED_TOLERANCE_10S ->
                floatValue.isFinite() && floatValue in 0.1f..5f
            Key.SPEED_LED_BRIGHTNESS -> unsignedValue in 5L..100L
            else -> false
        }
        if (!valid) return
        mutableState.value = when (key) {
            Key.WHEEL -> previous.copy(
                wheelCircumferenceM = floatValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.STOP_THRESHOLD -> previous.copy(
                stopThresholdKmh = floatValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.AUTO_PAUSE -> previous.copy(
                autoPauseEnabled = raw != 0,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.AUTO_PAUSE_DELAY -> previous.copy(
                autoPauseDelayMs = unsignedValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.LOG_INTERVAL -> previous.copy(
                logIntervalMs = unsignedValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.GRAPH_WINDOW -> previous.copy(
                graphWindowSeconds = unsignedValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.SPEED_LED_ENABLED -> previous.copy(
                speedLedEnabled = raw != 0,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.SPEED_LED_TOLERANCE_2S -> previous.copy(
                speedLedTolerance2sKmh = floatValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.SPEED_LED_TOLERANCE_5S -> previous.copy(
                speedLedTolerance5sKmh = floatValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.SPEED_LED_TOLERANCE_10S -> previous.copy(
                speedLedTolerance10sKmh = floatValue,
                loadedKeys = previous.loadedKeys + key,
            )
            Key.SPEED_LED_BRIGHTNESS -> previous.copy(
                speedLedBrightnessPercent = unsignedValue,
                loadedKeys = previous.loadedKeys + key,
            )
            else -> previous
        }
    }

    private fun handleResult(payload: ByteArray) {
        if (payload.size != 3) return
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        val key = buffer.short.toInt() and 0xFFFF
        val result = buffer.get().toInt() and 0xFF
        mutableState.value = mutableState.value.copy(
            lastResult = if (result == 0) "Setting $key saved" else "Setting $key rejected ($result)",
        )
    }
}
