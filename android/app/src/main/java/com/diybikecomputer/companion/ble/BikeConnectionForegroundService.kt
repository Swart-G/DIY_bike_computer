package com.diybikecomputer.companion.ble

import android.Manifest
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.diybikecomputer.companion.BikeComputerApplication
import com.diybikecomputer.companion.MainActivity
import com.diybikecomputer.companion.R
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch

/** Keeps the BLE GATT/reconnect loop alive while the UI process is backgrounded. */
class BikeConnectionForegroundService : Service() {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val connection: BikeConnectionService
        get() = (application as BikeComputerApplication).connection
    private var stateJob: Job? = null

    override fun onCreate() {
        super.onCreate()
        createChannel()
        startInForeground(BikeConnectionState.Reconnecting)
        stateJob = scope.launch {
            connection.state.collectLatest { state ->
                notificationManager().notify(NOTIFICATION_ID, notification(state))
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (hasBluetoothPermission()) connection.ensureConnected()
        return START_STICKY
    }

    override fun onDestroy() {
        stateJob?.cancel()
        scope.cancel()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun startInForeground(state: BikeConnectionState) {
        ServiceCompat.startForeground(
            this,
            NOTIFICATION_ID,
            notification(state),
            if (Build.VERSION.SDK_INT >= 29) {
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            } else {
                0
            },
        )
    }

    private fun notification(state: BikeConnectionState): Notification {
        val openApp = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
        )
        val text = when (state) {
            BikeConnectionState.Ready -> "Bike computer connected"
            BikeConnectionState.Connecting,
            BikeConnectionState.Initializing -> "Connecting to bike computer"
            BikeConnectionState.Unpaired -> "No paired bike computer"
            else -> "Waiting for bike computer"
        }
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_bike)
            .setContentTitle("Bike Computer")
            .setContentText(text)
            .setContentIntent(openApp)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()
    }

    private fun createChannel() {
        notificationManager().createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID,
                "Bike computer connection",
                NotificationManager.IMPORTANCE_LOW,
            ).apply {
                description = "Keeps the bike computer connected in the background"
                setShowBadge(false)
            },
        )
    }

    private fun notificationManager(): NotificationManager =
        getSystemService(NotificationManager::class.java)

    private fun hasBluetoothPermission(): Boolean =
        Build.VERSION.SDK_INT < 31 ||
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT,
            ) == PackageManager.PERMISSION_GRANTED

    companion object {
        private const val CHANNEL_ID = "bike_connection"
        private const val NOTIFICATION_ID = 2101

        fun start(context: Context) {
            ContextCompat.startForegroundService(
                context,
                Intent(context, BikeConnectionForegroundService::class.java),
            )
        }

        fun stop(context: Context) {
            context.stopService(
                Intent(context, BikeConnectionForegroundService::class.java),
            )
        }
    }
}
