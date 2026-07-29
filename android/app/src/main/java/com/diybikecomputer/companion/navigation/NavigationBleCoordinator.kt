package com.diybikecomputer.companion.navigation

import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.ble.BikeProtocol
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class NavigationBleCoordinator(
    private val connection: BikeConnectionService,
    private val repository: NavigationRepository,
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)

    init {
        scope.launch {
            repository.state.collect(::sendState)
        }
        scope.launch {
            connection.state.collect {
                if (it == BikeConnectionState.Ready) sendState(repository.state.value)
            }
        }
    }

    private fun sendState(nav: NavigationSnapshot) {
        if (connection.state.value != BikeConnectionState.Ready) return
        val street = utf8Prefix(nav.streetName, 64)
        val payload = ByteBuffer.allocate(25 + street.size)
            .order(ByteOrder.LITTLE_ENDIAN)
            .put(if (nav.available) 1 else 0)
            .put(nav.lifecycle.wireValue.toByte())
            .put(nav.maneuver.wireValue.toByte())
            .put(nav.nextManeuver.wireValue.toByte())
            .putInt(nav.distanceToManeuverM.coerceIn(0, 0xFFFF_FFFFL).toInt())
            .putInt(nav.nextDistanceM.coerceIn(0, 0xFFFF_FFFFL).toInt())
            .putInt(nav.remainingDistanceM.coerceIn(0, 0xFFFF_FFFFL).toInt())
            .putLong(nav.etaUtcMs)
            .put(street.size.toByte())
            .put(street)
            .array()
        connection.send(
            BikeProtocol.Message.NAVIGATION_STATE,
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
