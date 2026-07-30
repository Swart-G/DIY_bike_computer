package com.diybikecomputer.companion

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.ScanResult
import android.companion.CompanionDeviceManager
import android.content.pm.PackageManager
import android.content.ComponentName
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import android.provider.Settings
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
import com.diybikecomputer.companion.ui.BikeComputerApp
import com.diybikecomputer.companion.ui.theme.BikeComputerTheme
import com.diybikecomputer.companion.media.MediaBridgeService
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
    private var mediaAccessEnabled by mutableStateOf(false)
    private var pairingInProgress by mutableStateOf(false)
    private var pairingMessage by mutableStateOf<String?>(null)
    private var pendingExportRideId: String? = null
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
            app.gpsRepository.setEnabled(true)
            gpsEnabled = true
            app.gpsCoordinator.onEnabled()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        gpsEnabled = app.gpsRepository.enabled()
        mediaAccessEnabled = hasMediaAccess()
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
        if (hasNearbyPermission() && !connection.connectKnown()) {
            association.latestAssociatedDevice()?.let { device ->
                connection.connect(device, association.systemAssociationIdFor(device.address))
            }
        }
    }

    override fun onResume() {
        super.onResume()
        mediaAccessEnabled = hasMediaAccess()
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
        val permissions = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION)
        if (Build.VERSION.SDK_INT >= 33) {
            permissions += Manifest.permission.POST_NOTIFICATIONS
        }
        locationPermissions.launch(permissions.toTypedArray())
    }

    private fun handleGpsSetting(enabled: Boolean) {
        if (enabled) {
            requestGpsAssist()
        } else {
            app.gpsRepository.setEnabled(false)
            gpsEnabled = false
            app.gpsCoordinator.onDisabled()
        }
    }

    private fun forgetDevice(device: KnownDevice) {
        connection.forgetDevice(device.bluetoothAddress)
        association.disassociate(
            device.systemAssociationId
                ?: association.systemAssociationIdFor(device.bluetoothAddress),
            device.bluetoothAddress,
        )
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
