package com.diybikecomputer.companion.location

import android.content.Context
import android.content.Intent
import androidx.core.content.ContextCompat
import com.diybikecomputer.companion.ble.BikeConnectionService
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class RideGpsCoordinator(
    private val context: Context,
    private val connection: BikeConnectionService,
    private val gps: GpsRepository,
) {
    private val scope =
        CoroutineScope(SupervisorJob() + Dispatchers.IO.limitedParallelism(1))
    private var activeRideId: Long? = null

    init {
        scope.launch {
            connection.rideEvents.collect { event ->
                when (event.event) {
                    0 -> start(event.rideId)
                    3 -> stop()
                }
            }
        }
        scope.launch {
            connection.telemetry.collect { telemetry ->
                if (telemetry.rideState == 1 && telemetry.rideId != 0L) {
                    start(telemetry.rideId)
                } else if ((telemetry.rideState == 0 || telemetry.rideState == 3) &&
                    (activeRideId != null || RideLocationService.hasActiveSession(context))
                ) {
                    stop()
                }
            }
        }
    }

    fun onEnabled() {
        scope.launch {
            val telemetry = connection.telemetry.value
            if (telemetry.rideState == 1 && telemetry.rideId != 0L) {
                start(telemetry.rideId)
            }
        }
    }

    fun onDisabled() {
        scope.launch { stop() }
    }

    private suspend fun start(numericRideId: Long) {
        if (!gps.enabled() || numericRideId !in 1..0xFFFF_FFFFL) return
        if (activeRideId == numericRideId) return
        activeRideId = numericRideId
        val started = runCatching {
            ContextCompat.startForegroundService(
                context,
                Intent(context, RideLocationService::class.java)
                    .setAction(RideLocationService.ACTION_START)
                    .putExtra(RideLocationService.EXTRA_RIDE_ID, numericRideId),
            )
        }.getOrNull() != null
        if (!started) {
            activeRideId = null
            gps.setRecordingStatus("Android refused to start location forwarding")
        }
    }

    private fun stop() {
        activeRideId = null
        // Direct stop is legal from the background and avoids trying to start
        // a second service command merely to stop an existing foreground service.
        RideLocationService.forceStop(context)
    }
}
