package com.diybikecomputer.companion

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.ScanResult
import android.companion.CompanionDeviceManager
import android.content.pm.PackageManager
import android.content.ComponentName
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import android.provider.Settings
import android.service.notification.NotificationListenerService
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.diybikecomputer.companion.device.CompanionDevicePairing
import com.diybikecomputer.companion.device.KnownDevice
import com.diybikecomputer.companion.ble.BikeConnectionForegroundService
import com.diybikecomputer.companion.ui.BikeComputerApp
import com.diybikecomputer.companion.ui.theme.BikeComputerTheme
import com.diybikecomputer.companion.media.MediaBridgeService
import com.diybikecomputer.companion.location.LocationPermissionAction
import com.diybikecomputer.companion.location.LocationPermissionPolicy
import com.diybikecomputer.companion.rides.RideExporter
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {
    private val connection by lazy {
        (application as BikeComputerApplication).connection
    }
    private val association by lazy { CompanionDevicePairing(this) }
    private val app by lazy { application as BikeComputerApplication }
    private var gpsEnabled by mutableStateOf(false)
    private var gpsStatusMessage by mutableStateOf("Off")
    private var mediaAccessEnabled by mutableStateOf(false)
    private var pairingInProgress by mutableStateOf(false)
    private var pairingMessage by mutableStateOf<String?>(null)
    private var pendingExportRideId: String? = null
    private var notificationPermissionRequested = false
    private val exporter by lazy { RideExporter(contentResolver, app.database) }
    private val csvExport = registerForActivityResult(
        ActivityResultContracts.CreateDocument("text/csv"),
    ) { uri ->
        val rideId = pendingExportRideId
        if (uri != null && rideId != null) {
            launchExport { exporter.exportCsv(rideId, uri) }
        }
        pendingExportRideId = null
    }
    private val gpxExport = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/gpx+xml"),
    ) { uri ->
        val rideId = pendingExportRideId
        if (uri != null && rideId != null) {
            launchExport { exporter.exportGpx(rideId, uri) }
        }
        pendingExportRideId = null
    }
    private val xlsxExport = registerForActivityResult(
        ActivityResultContracts.CreateDocument(
            "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        ),
    ) { uri ->
        val rideId = pendingExportRideId
        if (uri != null && rideId != null) {
            launchExport { exporter.exportSummaryXlsx(rideId, uri) }
        }
        pendingExportRideId = null
    }
    private val chooser = registerForActivityResult(
        ActivityResultContracts.StartIntentSenderForResult(),
    ) { result ->
        if (Build.VERSION.SDK_INT >= 33) {
            if (result.resultCode != Activity.RESULT_OK) {
                pairingInProgress = false
                pairingMessage = "No device was selected"
            }
            return@registerForActivityResult
        }
        @Suppress("DEPRECATION")
        val selected: Parcelable? =
            result.data?.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE)
        val device = when (selected) {
            is BluetoothDevice -> selected
            is ScanResult -> selected.device
            else -> null
        }
        pairingInProgress = false
        if (device != null) {
            connection.connect(
                device,
                association.systemAssociationIdFor(device.address),
            )
            activateBackgroundConnection()
        } else {
            pairingMessage = "No device was selected"
        }
    }
    private val nearbyPermissions = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { grants ->
        if (grants.values.all { it }) {
            beginAssociation()
        } else {
            pairingInProgress = false
            pairingMessage = "Nearby devices permission is required for pairing"
        }
    }
    private val locationPermissions = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { grants ->
        val fineGranted =
            grants[Manifest.permission.ACCESS_FINE_LOCATION] == true ||
                ContextCompat.checkSelfPermission(
                    this,
                    Manifest.permission.ACCESS_FINE_LOCATION,
                ) == PackageManager.PERMISSION_GRANTED
        if (fineGranted) {
            requestBackgroundLocationIfNeeded()
        } else {
            disableGpsAssist("Precise location permission is required")
        }
    }
    private val backgroundLocationPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        if (granted || hasBackgroundLocationPermission()) {
            enableGpsAssist()
        } else {
            disableGpsAssist("Allow all the time is required for reliable ride forwarding")
        }
    }
    private val backgroundLocationSettings = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult(),
    ) {
        if (hasBackgroundLocationPermission()) {
            enableGpsAssist()
        } else {
            disableGpsAssist("Location permission is not set to Allow all the time")
        }
    }
    private val reconnectPermissions = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) {
        if (hasNearbyPermission()) ensureKnownDeviceConnection()
    }
    private val notificationPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) {
        // A foreground service remains legal when notifications are denied,
        // but asking keeps its persistent connection state visible to the user.
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        refreshGpsPermissionState()
        refreshMediaAccess()
        setContent {
            BikeComputerTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    BikeComputerApp(
                        connection = connection,
                        mediaRepository = app.mediaRepository,
                        navigationRepository = app.navigationRepository,
                        deviceSettingsRepository = app.deviceSettings,
                        database = app.database,
                        rideSync = app.rideSync,
                        gpsEnabled = gpsEnabled,
                        gpsStatusMessage = gpsStatusMessage,
                        mediaAccessEnabled = mediaAccessEnabled,
                        pairingInProgress = pairingInProgress,
                        pairingMessage = pairingMessage,
                        onPair = ::requestPairing,
                        onConnectDevice = { connection.connectKnown(it) },
                        onForgetDevice = ::forgetDevice,
                        onGpsChanged = ::handleGpsSetting,
                        onEnableMedia = {
                            startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
                        },
                        onMediaPlayerSelected = app.mediaRepository::setPreferredPlayer,
                        onExportCsv = {
                            pendingExportRideId = it
                            csvExport.launch("ride_${it.substringAfterLast(':')}.csv")
                        },
                        onExportXlsx = {
                            pendingExportRideId = it
                            xlsxExport.launch("ride_${it.substringAfterLast(':')}_summary.xlsx")
                        },
                        onExportGpx = {
                            pendingExportRideId = it
                            gpxExport.launch("ride_${it.substringAfterLast(':')}.gpx")
                        },
                    )
                }
            }
        }
        if (hasNearbyPermission()) {
            ensureKnownDeviceConnection()
        } else if (connection.knownDevice() != null) {
            reconnectPermissions.launch(requiredNearbyPermissions())
        }
    }

    override fun onResume() {
        super.onResume()
        refreshMediaAccess()
        refreshGpsPermissionState()
    }

    private fun requestPairing() {
        if (pairingInProgress) return
        pairingInProgress = true
        pairingMessage = null
        if (hasNearbyPermission()) {
            beginAssociation()
        } else {
            nearbyPermissions.launch(requiredNearbyPermissions())
        }
    }

    private fun requestGpsAssist() {
        when (
            LocationPermissionPolicy.nextAction(
                Build.VERSION.SDK_INT,
                hasPreciseLocationPermission(),
                hasBackgroundLocationPermission(),
            )
        ) {
            LocationPermissionAction.RequestForeground -> {
                gpsStatusMessage = "Grant precise location to forward it to the bike"
                // Android 12+ ignores a fine-only request; coarse and fine must
                // be requested together.
                locationPermissions.launch(
                    arrayOf(
                        Manifest.permission.ACCESS_COARSE_LOCATION,
                        Manifest.permission.ACCESS_FINE_LOCATION,
                    ),
                )
            }
            LocationPermissionAction.RequestBackgroundPermission -> {
                gpsStatusMessage =
                    "Grant background location for rides started with the app closed"
                if (Build.VERSION.SDK_INT >= 29) {
                    backgroundLocationPermission.launch(
                        Manifest.permission.ACCESS_BACKGROUND_LOCATION,
                    )
                } else {
                    // Defensive fallback: the pure policy currently never selects
                    // this branch below Android 10.
                    enableGpsAssist()
                }
            }
            LocationPermissionAction.OpenAppSettings -> openBackgroundLocationSettings()
            LocationPermissionAction.Enable -> enableGpsAssist()
        }
    }

    private fun handleGpsSetting(enabled: Boolean) {
        if (enabled) {
            requestGpsAssist()
        } else {
            disableGpsAssist("Off")
        }
    }

    private fun requestBackgroundLocationIfNeeded() {
        requestGpsAssist()
    }

    private fun openBackgroundLocationSettings() {
        gpsStatusMessage = "Grant background location for rides started with the app closed"
        val optionLabel = if (Build.VERSION.SDK_INT >= 30) {
            packageManager.backgroundPermissionOptionLabel
        } else {
            "Allow all the time"
        }
        Toast.makeText(
            this,
            "Location permission: select $optionLabel",
            Toast.LENGTH_LONG,
        ).show()
        backgroundLocationSettings.launch(
            Intent(
                Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                Uri.parse("package:$packageName"),
            ),
        )
    }

    private fun enableGpsAssist() {
        if (!hasPreciseLocationPermission() || !hasBackgroundLocationPermission()) {
            disableGpsAssist("Required location permission is missing")
            return
        }
        app.gpsRepository.setEnabled(true)
        gpsEnabled = true
        gpsStatusMessage = app.gpsRepository.recordingStatus()
            .takeUnless { it.isBlank() || it == "Off" }
            ?: "Ready · sends only during an active bike ride"
        requestNotificationPermissionIfNeeded()
        app.gpsCoordinator.onEnabled()
    }

    private fun disableGpsAssist(message: String) {
        app.gpsRepository.setEnabled(false)
        gpsEnabled = false
        gpsStatusMessage = message
        app.gpsCoordinator.onDisabled()
    }

    private fun refreshGpsPermissionState() {
        if (!app.gpsRepository.enabled()) {
            gpsEnabled = false
            if (gpsStatusMessage == "Ready · sends only during an active bike ride") {
                gpsStatusMessage = "Off"
            }
            return
        }
        if (hasPreciseLocationPermission() && hasBackgroundLocationPermission()) {
            gpsEnabled = true
            gpsStatusMessage = app.gpsRepository.recordingStatus()
                .takeUnless { it.isBlank() || it == "Off" }
                ?: "Ready · sends only during an active bike ride"
        } else {
            disableGpsAssist("Permission was revoked; enable GPS Assist again")
        }
    }

    private fun hasPreciseLocationPermission(): Boolean =
        ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION,
        ) == PackageManager.PERMISSION_GRANTED

    private fun hasBackgroundLocationPermission(): Boolean =
        Build.VERSION.SDK_INT < 29 ||
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_BACKGROUND_LOCATION,
            ) == PackageManager.PERMISSION_GRANTED

    private fun forgetDevice(device: KnownDevice) {
        connection.forgetDevice(device.bluetoothAddress)
        association.disassociate(
            device.systemAssociationId
                ?: association.systemAssociationIdFor(device.bluetoothAddress),
            device.bluetoothAddress,
        )
        if (connection.knownDevice() == null) {
            BikeConnectionForegroundService.stop(this)
        }
    }

    private fun beginAssociation() {
        association.associate(
            object : CompanionDevicePairing.Callback {
                override fun onChooser(intentSender: android.content.IntentSender) {
                    pairingMessage = null
                    chooser.launch(IntentSenderRequest.Builder(intentSender).build())
                }

                override fun onAssociated(association: android.companion.AssociationInfo) {
                    pairingInProgress = false
                    pairingMessage = null
                    this@MainActivity.association.deviceFor(association)?.let {
                        connection.connect(
                            it,
                            if (Build.VERSION.SDK_INT >= 33) association.id else null,
                        )
                        activateBackgroundConnection()
                    }
                        ?: run {
                            pairingMessage = "Associated device address is unavailable"
                        }
                }

                override fun onFailure(reason: String) {
                    pairingInProgress = false
                    pairingMessage = reason
                }
            },
        )
    }

    private fun hasNearbyPermission(): Boolean =
        requiredNearbyPermissions().all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }

    private fun requiredNearbyPermissions(): Array<String> =
        if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    private fun hasMediaAccess(): Boolean {
        val expected = ComponentName(this, MediaBridgeService::class.java)
        return Settings.Secure.getString(
            contentResolver,
            "enabled_notification_listeners",
        ).orEmpty().split(':').any {
            ComponentName.unflattenFromString(it) == expected
        }
    }

    private fun ensureKnownDeviceConnection() {
        val started = connection.ensureConnected()
        var hasDevice = connection.knownDevice() != null
        if (!started) {
            association.latestAssociatedDevice()?.let { device ->
                connection.connect(
                    device,
                    association.systemAssociationIdFor(device.address),
                )
                hasDevice = true
            }
        }
        if (started || hasDevice) activateBackgroundConnection()
    }

    private fun activateBackgroundConnection() {
        BikeConnectionForegroundService.start(this)
        requestNotificationPermissionIfNeeded()
    }

    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT < 33 || notificationPermissionRequested ||
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.POST_NOTIFICATIONS,
            ) == PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        notificationPermissionRequested = true
        notificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
    }

    private fun refreshMediaAccess() {
        mediaAccessEnabled = hasMediaAccess()
        if (mediaAccessEnabled) {
            NotificationListenerService.requestRebind(
                ComponentName(this, MediaBridgeService::class.java),
            )
        }
    }

    private fun launchExport(export: suspend () -> Boolean) {
        lifecycleScope.launch {
            val success = try {
                export()
            } catch (cancelled: CancellationException) {
                throw cancelled
            } catch (_: Exception) {
                false
            }
            Toast.makeText(
                this@MainActivity,
                if (success) "Export complete" else "Export failed",
                Toast.LENGTH_LONG,
            ).show()
        }
    }
}
