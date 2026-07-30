package com.diybikecomputer.companion.location

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.diybikecomputer.companion.BikeComputerApplication
import com.diybikecomputer.companion.R
import com.diybikecomputer.companion.rides.GpsPointEntity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.util.concurrent.atomic.AtomicInteger

class RideLocationService : Service(), LocationListener {
    private val scope =
        CoroutineScope(SupervisorJob() + Dispatchers.IO.limitedParallelism(1))
    private val startGeneration = AtomicInteger()
    private lateinit var locationManager: LocationManager
    private var rideId: String? = null
    private var nextPointIndex = 0L

    override fun onCreate() {
        super.onCreate()
        locationManager = getSystemService(LocationManager::class.java)
        createChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopRecording()
            return START_NOT_STICKY
        }
        val requestedRide = intent?.getStringExtra(EXTRA_RIDE_ID)
            ?: getSharedPreferences(PREFERENCES, MODE_PRIVATE)
                .getString(ACTIVE_RIDE, null)
        if (requestedRide.isNullOrBlank()) {
            stopSelf()
            return START_NOT_STICKY
        }
        rideId = requestedRide
        getSharedPreferences(PREFERENCES, MODE_PRIVATE)
            .edit().putString(ACTIVE_RIDE, requestedRide).apply()
        startForeground(
            NOTIFICATION_ID,
            NotificationCompat.Builder(this, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_bike)
                .setContentTitle("Recording bike route")
                .setContentText("GPS Assist · wheel sensor remains authoritative")
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .build(),
        )
        val generation = startGeneration.incrementAndGet()
        scope.launch {
            val storedPointIndex =
                (application as BikeComputerApplication).database.rideDao()
                    .lastGpsPointIndex(requestedRide) + 1
            if (generation != startGeneration.get() || rideId != requestedRide) {
                return@launch
            }
            nextPointIndex = storedPointIndex
            startLocationUpdates()
        }
        return START_STICKY
    }

    override fun onLocationChanged(location: Location) {
        val activeRide = rideId ?: return
        val pointIndex = nextPointIndex++
        scope.launch {
            (application as BikeComputerApplication).database.rideDao().upsertGpsPoints(
                listOf(
                    GpsPointEntity(
                        rideId = activeRide,
                        pointIndex = pointIndex,
                        timestampUtcMs = location.time,
                        latitude = location.latitude,
                        longitude = location.longitude,
                        altitudeM = location.altitude.takeIf { location.hasAltitude() },
                        accuracyM = location.accuracy.takeIf { location.hasAccuracy() },
                        diagnosticGpsSpeedMps =
                            location.speed.takeIf { location.hasSpeed() },
                    ),
                ),
            )
        }
    }

    override fun onProviderDisabled(provider: String) = Unit
    override fun onProviderEnabled(provider: String) = Unit
    @Deprecated("LocationManager no longer reports provider status this way")
    override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) = Unit
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        locationManager.removeUpdates(this)
        scope.cancel()
        super.onDestroy()
    }

    private fun startLocationUpdates() {
        if (ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_FINE_LOCATION,
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            stopRecording()
            return
        }
        runCatching {
            locationManager.removeUpdates(this)
            locationManager.requestLocationUpdates(
                LocationManager.GPS_PROVIDER,
                2000L,
                3f,
                this,
                mainLooper,
            )
        }.onFailure { stopRecording() }
    }

    private fun stopRecording() {
        startGeneration.incrementAndGet()
        locationManager.removeUpdates(this)
        rideId = null
        getSharedPreferences(PREFERENCES, MODE_PRIVATE)
            .edit().remove(ACTIVE_RIDE).apply()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun createChannel() {
        getSystemService(NotificationManager::class.java).createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                "Ride GPS recording",
                NotificationManager.IMPORTANCE_LOW,
            ),
        )
    }

    companion object {
        const val ACTION_START =
            "com.diybikecomputer.companion.location.START"
        const val ACTION_STOP =
            "com.diybikecomputer.companion.location.STOP"
        const val EXTRA_RIDE_ID = "ride_id"
        private const val PREFERENCES = "gps_active_session"
        private const val ACTIVE_RIDE = "active_ride"
        private const val CHANNEL_ID = "ride_gps"
        private const val NOTIFICATION_ID = 42

        fun forceStop(context: android.content.Context) {
            context.getSharedPreferences(PREFERENCES, android.content.Context.MODE_PRIVATE)
                .edit().remove(ACTIVE_RIDE).apply()
            context.stopService(Intent(context, RideLocationService::class.java))
        }
    }
}
