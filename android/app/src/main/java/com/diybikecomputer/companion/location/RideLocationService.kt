package com.diybikecomputer.companion.location

import android.Manifest
import android.annotation.SuppressLint
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.os.IBinder
import android.os.SystemClock
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.diybikecomputer.companion.BikeComputerApplication
import com.diybikecomputer.companion.R
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.ble.BikeProtocol

class RideLocationService : Service(), LocationListener {
    private lateinit var locationManager: LocationManager
    private val gpsRepository by lazy { GpsRepository(this) }
    private var rideId: Long = 0
    private var lastStatus = ""

    override fun onCreate() {
        super.onCreate()
        locationManager = getSystemService(LocationManager::class.java)
        createChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopRecording("Off")
            return START_NOT_STICKY
        }
        val requestedRide = intent?.getLongExtra(EXTRA_RIDE_ID, 0L)
            ?.takeIf { it != 0L }
            ?: getSharedPreferences(PREFERENCES, MODE_PRIVATE)
                .getLong(ACTIVE_RIDE, 0L)
        if (requestedRide !in 1..0xFFFF_FFFFL) {
            gpsRepository.setRecordingStatus("No active ride")
            stopSelf()
            return START_NOT_STICKY
        }
        rideId = requestedRide
        getSharedPreferences(PREFERENCES, MODE_PRIVATE)
            .edit().putLong(ACTIVE_RIDE, requestedRide).apply()
        if (!hasRequiredLocationPermissions()) {
            stopRecording("Location permission is missing")
            return START_NOT_STICKY
        }
        try {
            ServiceCompat.startForeground(
                this,
                NOTIFICATION_ID,
                notification("Starting phone location for the active ride"),
                if (android.os.Build.VERSION.SDK_INT >= 29) {
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION
                } else {
                    0
                },
            )
        } catch (_: SecurityException) {
            stopRecording("Android blocked background location")
            return START_NOT_STICKY
        }
        setStatus("Starting phone location…", "Waiting for a location fix")
        startLocationUpdates()
        return START_STICKY
    }

    override fun onLocationChanged(location: Location) {
        val activeRide = rideId.takeIf { it != 0L } ?: return
        if (location.latitude !in -90.0..90.0 ||
            location.longitude !in -180.0..180.0 || location.time <= 0L ||
            !isFreshSystemFix(location)
        ) {
            return
        }
        val app = application as BikeComputerApplication
        if (app.connection.state.value != BikeConnectionState.Ready) {
            setStatus(
                "Bike disconnected · location is not stored",
                "Waiting for bike connection; location is not stored",
            )
            return
        }
        val payload = PhoneLocationCodec.encode(
            PhoneLocationFix(
                rideId = activeRide,
                timestampUtcMs = location.time,
                latitude = location.latitude,
                longitude = location.longitude,
                altitudeM = location.altitude.takeIf { location.hasAltitude() },
                accuracyM = location.accuracy.takeIf { location.hasAccuracy() },
                speedMps = location.speed.takeIf { location.hasSpeed() },
            ),
        )
        if (payload == null) {
            setStatus("Invalid phone location was rejected", "Waiting for a valid fix")
            return
        }
        val queued = app.connection.send(
            BikeProtocol.Message.LOCATION_FIX,
            BikeProtocol.Flag.PRIVILEGED,
            payload,
        )
        if (queued) {
            setStatus(
                "Sending location to bike · nothing stored on phone",
                "Latest location sent to bike",
            )
        } else {
            setStatus(
                "BLE busy · location discarded, not stored",
                "Location was not queued; waiting for next fix",
            )
        }
    }

    override fun onProviderDisabled(provider: String) {
        startLocationUpdates()
    }
    override fun onProviderEnabled(provider: String) {
        startLocationUpdates()
    }
    @Deprecated("LocationManager no longer reports provider status this way")
    override fun onStatusChanged(provider: String?, status: Int, extras: Bundle?) = Unit
    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        locationManager.removeUpdates(this)
        super.onDestroy()
    }

    @SuppressLint("MissingPermission")
    private fun startLocationUpdates() {
        if (!hasRequiredLocationPermissions()) {
            stopRecording("Location permission was revoked")
            return
        }
        runCatching {
            locationManager.removeUpdates(this)
            var providerRequested = false
            if (locationManager.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                locationManager.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER,
                    2000L,
                    3f,
                    this,
                    mainLooper,
                )
                providerRequested = true
            }
            if (locationManager.isProviderEnabled(LocationManager.NETWORK_PROVIDER)) {
                locationManager.requestLocationUpdates(
                    LocationManager.NETWORK_PROVIDER,
                    5000L,
                    10f,
                    this,
                    mainLooper,
                )
                providerRequested = true
            }
            if (providerRequested) {
                setStatus("Forwarding enabled · waiting for a fix", "Waiting for a location fix")
            } else {
                setStatus(
                    "Waiting for phone Location services",
                    "Turn on phone Location services",
                )
            }
        }.onFailure {
            stopRecording("Unable to request phone location")
        }
    }

    private fun stopRecording(status: String) {
        locationManager.removeUpdates(this)
        rideId = 0
        getSharedPreferences(PREFERENCES, MODE_PRIVATE)
            .edit().remove(ACTIVE_RIDE).apply()
        gpsRepository.setRecordingStatus(status)
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    private fun hasRequiredLocationPermissions(): Boolean {
        val fine = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION,
        ) == PackageManager.PERMISSION_GRANTED
        val background = android.os.Build.VERSION.SDK_INT < 29 ||
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_BACKGROUND_LOCATION,
            ) == PackageManager.PERMISSION_GRANTED
        return fine && background
    }

    private fun notification(text: String) =
        NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_bike)
            .setContentTitle("Sending location to bike computer")
            .setContentText(text)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .build()

    private fun updateNotification(text: String) {
        getSystemService(NotificationManager::class.java)
            .notify(NOTIFICATION_ID, notification(text))
    }

    private fun setStatus(status: String, notificationText: String) {
        if (status == lastStatus) return
        lastStatus = status
        gpsRepository.setRecordingStatus(status)
        updateNotification(notificationText)
    }

    private fun isFreshSystemFix(location: Location): Boolean {
        val fixElapsedNanos = location.elapsedRealtimeNanos
        if (fixElapsedNanos <= 0L) return true
        val ageNanos = SystemClock.elapsedRealtimeNanos() - fixElapsedNanos
        return ageNanos in -1_000_000_000L..10_000_000_000L
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
            GpsRepository(context).setRecordingStatus("Off")
            context.stopService(Intent(context, RideLocationService::class.java))
        }

        fun hasActiveSession(context: android.content.Context): Boolean =
            context.getSharedPreferences(PREFERENCES, android.content.Context.MODE_PRIVATE)
                .getLong(ACTIVE_RIDE, 0L) != 0L
    }
}
