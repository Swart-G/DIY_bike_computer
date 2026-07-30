package com.diybikecomputer.companion.location

import android.content.Context
import android.content.Intent
import androidx.core.content.ContextCompat
import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.rides.DeviceEntity
import com.diybikecomputer.companion.rides.RideDatabase
import com.diybikecomputer.companion.rides.RideEntity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch

class RideGpsCoordinator(
    private val context: Context,
    private val connection: BikeConnectionService,
    private val database: RideDatabase,
    private val gps: GpsRepository,
) {
    private val scope =
        CoroutineScope(SupervisorJob() + Dispatchers.IO.limitedParallelism(1))
    private var activeRideKey: String? = null

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
                    activeRideKey != null
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
        if (!gps.enabled()) return
        val device = connection.knownDevice() ?: return
        val rideKey = "${java.lang.Long.toUnsignedString(device.deviceId)}:$numericRideId"
        if (activeRideKey == rideKey) return
        database.deviceDao().upsert(
            DeviceEntity(
                deviceId = device.deviceId,
                associationId = device.associationId,
                displayName = device.displayName,
                lastSeenUtcMs = System.currentTimeMillis(),
                protocolVersion = 1,
            ),
        )
        if (database.rideDao().getRide(rideKey) == null) {
            database.rideDao().upsertRide(
                RideEntity(
                    rideId = rideKey,
                    deviceId = device.deviceId,
                    formatVersion = 1,
                    startedAtUtcMs = System.currentTimeMillis(),
                    finishedAtUtcMs = null,
                    finished = false,
                    distanceM = 0.0,
                    movingTimeMs = 0,
                    elapsedTimeMs = 0,
                    averageSpeedKmh = 0.0,
                    maxSpeedKmh = 0.0,
                    syncRevision = 0,
                    synced = false,
                ),
            )
        }
        activeRideKey = rideKey
        ContextCompat.startForegroundService(
            context,
            Intent(context, RideLocationService::class.java)
                .setAction(RideLocationService.ACTION_START)
                .putExtra(RideLocationService.EXTRA_RIDE_ID, rideKey),
        )
    }

    private fun stop() {
        activeRideKey = null
        context.startService(
            Intent(context, RideLocationService::class.java)
                .setAction(RideLocationService.ACTION_STOP),
        )
    }
}
