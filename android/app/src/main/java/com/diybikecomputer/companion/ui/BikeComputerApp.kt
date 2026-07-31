package com.diybikecomputer.companion.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.ble.LiveTelemetry
import com.diybikecomputer.companion.device.KnownDevice
import com.diybikecomputer.companion.media.MediaPlayerOption
import com.diybikecomputer.companion.media.MediaRepository
import com.diybikecomputer.companion.navigation.NavigationRepository
import com.diybikecomputer.companion.rides.RideDatabase
import com.diybikecomputer.companion.rides.RideSyncManager
import com.diybikecomputer.companion.settings.DeviceSettings
import com.diybikecomputer.companion.settings.DeviceSettingsRepository
import com.diybikecomputer.companion.ui.history.HistoryScreen
import com.diybikecomputer.companion.ui.history.RideDetailScreen

private enum class AppPage(val label: String) {
    Home("Home"),
    History("History"),
    Settings("Settings"),
}

@Composable
fun BikeComputerApp(
    connection: BikeConnectionService,
    mediaRepository: MediaRepository,
    navigationRepository: NavigationRepository,
    deviceSettingsRepository: DeviceSettingsRepository,
    database: RideDatabase,
    rideSync: RideSyncManager,
    gpsEnabled: Boolean,
    gpsStatusMessage: String,
    mediaAccessEnabled: Boolean,
    pairingInProgress: Boolean,
    pairingMessage: String?,
    onPair: () -> Unit,
    onConnectDevice: (String) -> Unit,
    onForgetDevice: (KnownDevice) -> Unit,
    onGpsChanged: (Boolean) -> Unit,
    onEnableMedia: () -> Unit,
    onMediaPlayerSelected: (String?) -> Unit,
    onExportCsv: (String) -> Unit,
    onExportXlsx: (String) -> Unit,
    onExportGpx: (String) -> Unit,
) {
    val connectionState by connection.state.collectAsStateWithLifecycle()
    val knownDevices by connection.knownDevices.collectAsStateWithLifecycle()
    val telemetry by connection.telemetry.collectAsStateWithLifecycle()
    val media by mediaRepository.state.collectAsStateWithLifecycle()
    val mediaPlayers by mediaRepository.players.collectAsStateWithLifecycle()
    val preferredMediaPlayer by mediaRepository.preferredPackage.collectAsStateWithLifecycle()
    val navigation by navigationRepository.state.collectAsStateWithLifecycle()
    val settings by deviceSettingsRepository.state.collectAsStateWithLifecycle()
    val sync by rideSync.progress.collectAsStateWithLifecycle()
    val ready = connectionState == BikeConnectionState.Ready
    var page by remember { mutableStateOf(AppPage.Home) }
    var selectedRide by remember { mutableStateOf<String?>(null) }
    var forgetCandidate by remember { mutableStateOf<KnownDevice?>(null) }

    forgetCandidate?.let { device ->
        AlertDialog(
            onDismissRequest = { forgetCandidate = null },
            title = { Text("Forget ${device.displayName}?") },
            text = {
                Text(
                    "The Android association and automatic reconnect entry will be removed. " +
                        "Ride history stays on this phone.",
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        onForgetDevice(device)
                        forgetCandidate = null
                    },
                ) {
                    Text("Forget", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = {
                TextButton(onClick = { forgetCandidate = null }) { Text("Cancel") }
            },
        )
    }

    Scaffold(
        bottomBar = {
            NavigationBar {
                AppPage.entries.forEach { item ->
                    NavigationBarItem(
                        selected = page == item,
                        onClick = {
                            selectedRide = null
                            page = item
                        },
                        icon = {
                            Icon(
                                imageVector = when (item) {
                                    AppPage.Home -> Icons.Rounded.Home
                                    AppPage.History -> Icons.Rounded.History
                                    AppPage.Settings -> Icons.Rounded.Settings
                                },
                                contentDescription = item.label,
                            )
                        },
                        label = { Text(item.label) },
                    )
                }
            }
        },
    ) { padding ->
        Column(
            modifier = Modifier.fillMaxSize().padding(padding).padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Text(
                when {
                    selectedRide != null -> "Ride details"
                    page == AppPage.Home -> "Bike Computer"
                    page == AppPage.History -> "History"
                    else -> "Settings"
                },
                fontSize = 27.sp,
                fontWeight = FontWeight.Bold,
            )
            val activeRideId = selectedRide
            Box(modifier = Modifier.fillMaxWidth().weight(1f)) {
                when {
                    activeRideId != null -> RideDetailScreen(
                        dao = database.rideDao(),
                        rideId = activeRideId,
                        onBack = { selectedRide = null },
                        onExportCsv = onExportCsv,
                        onExportXlsx = onExportXlsx,
                        onExportGpx = onExportGpx,
                    )
                    page == AppPage.Home && ready -> ConnectedHome(
                        deviceName = connection.knownDevice()?.displayName ?: "Bike computer",
                        telemetry = telemetry,
                    )
                    page == AppPage.Home -> DisconnectedHome(
                        connectionState = connectionState,
                        devices = knownDevices,
                        target = connection.knownDevice(),
                        pairingInProgress = pairingInProgress,
                        pairingMessage = pairingMessage,
                        onPair = onPair,
                        onConnectDevice = onConnectDevice,
                        onForgetDevice = { forgetCandidate = it },
                    )
                    page == AppPage.History -> HistoryScreen(database.rideDao()) {
                        selectedRide = it
                    }
                    else -> SettingsPage(
                        ready = ready,
                        connectionState = connectionState,
                        devices = knownDevices,
                        activeBluetoothAddress = connection.knownDevice()?.bluetoothAddress,
                        settings = settings,
                        telemetry = telemetry,
                        gpsEnabled = gpsEnabled,
                        gpsStatusMessage = gpsStatusMessage,
                        mediaAccessEnabled = mediaAccessEnabled,
                        mediaPlayers = mediaPlayers,
                        preferredMediaPlayer = preferredMediaPlayer,
                        currentMedia = when {
                            media.available ->
                                "${media.player} · ${media.title.ifBlank { "active session" }}"
                            preferredMediaPlayer != null -> "Waiting for selected player"
                            else -> "No active media session"
                        },
                        navigationStatus = if (navigation.available) {
                            "${navigation.lifecycle.name} · ${navigation.streetName}"
                        } else {
                            "Experimental · provider unavailable"
                        },
                        syncStatus =
                            "${sync.state.name} · ${sync.downloadedBytes}/${sync.totalBytes}",
                        pairingInProgress = pairingInProgress,
                        onPair = onPair,
                        onConnectDevice = onConnectDevice,
                        onForgetDevice = { forgetCandidate = it },
                        onGpsChanged = onGpsChanged,
                        onEnableMedia = onEnableMedia,
                        onMediaPlayerSelected = onMediaPlayerSelected,
                        repository = deviceSettingsRepository,
                    )
                }
            }
        }
    }
}

@Composable
private fun ConnectedHome(deviceName: String, telemetry: LiveTelemetry) {
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Text(
            "●  $deviceName",
            color = MaterialTheme.colorScheme.primary,
            fontWeight = FontWeight.SemiBold,
        )
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer,
            ),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(22.dp),
                verticalAlignment = Alignment.Bottom,
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        "CURRENT SPEED",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.7f),
                    )
                    Text(
                        "%.1f".format(telemetry.speedKmh),
                        fontSize = 54.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                    Text("km/h", color = MaterialTheme.colorScheme.onPrimaryContainer)
                }
                Column(horizontalAlignment = Alignment.End) {
                    Text(
                        "${telemetry.batteryPercent}%",
                        fontSize = 29.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                    Text(
                        "battery",
                        color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.7f),
                    )
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            MetricCard(
                "DISTANCE",
                "%.2f km".format(telemetry.distanceM / 1000f),
                Modifier.weight(1f),
            )
            MetricCard(
                "AVERAGE",
                "%.1f km/h".format(telemetry.averageSpeedKmh),
                Modifier.weight(1f),
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            MetricCard(
                "MAXIMUM",
                "%.1f km/h".format(telemetry.maxSpeedKmh),
                Modifier.weight(1f),
            )
            MetricCard("MOVING", duration(telemetry.movingTimeMs), Modifier.weight(1f))
        }
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(7.dp)) {
                Text(rideStateLabel(telemetry.rideState), fontWeight = FontWeight.Bold)
                Text(
                    "Elapsed ${duration(telemetry.elapsedTimeMs)} · " +
                        "${if (telemetry.motionState == 1) "Auto paused" else "Moving"}",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    "Storage ${storageStateLabel(telemetry.sdState)} · " +
                        "${telemetry.pulseCount} accepted pulses",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun DisconnectedHome(
    connectionState: BikeConnectionState,
    devices: List<KnownDevice>,
    target: KnownDevice?,
    pairingInProgress: Boolean,
    pairingMessage: String?,
    onPair: () -> Unit,
    onConnectDevice: (String) -> Unit,
    onForgetDevice: (KnownDevice) -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surfaceVariant,
            ),
        ) {
            Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    target?.let { "Looking for ${it.displayName}" }
                        ?: "No bike computer selected",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                )
                Text(
                    connectionStatus(connectionState),
                    color = connectionColor(connectionState),
                    fontWeight = FontWeight.SemiBold,
                )
                Text(
                    connectionHint(connectionState, target != null),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Button(onClick = onPair, enabled = !pairingInProgress) {
                    Text(if (pairingInProgress) "Searching…" else "Add bike computer")
                }
                pairingMessage?.let {
                    Text(it, color = MaterialTheme.colorScheme.error)
                }
            }
        }
        DeviceList(
            devices = devices,
            ready = false,
            activeBluetoothAddress = target?.bluetoothAddress,
            onPair = onPair,
            onConnect = onConnectDevice,
            onForget = onForgetDevice,
            showAddButton = false,
        )
    }
}

@Composable
private fun SettingsPage(
    ready: Boolean,
    connectionState: BikeConnectionState,
    devices: List<KnownDevice>,
    activeBluetoothAddress: String?,
    settings: DeviceSettings,
    telemetry: LiveTelemetry,
    gpsEnabled: Boolean,
    gpsStatusMessage: String,
    mediaAccessEnabled: Boolean,
    mediaPlayers: List<MediaPlayerOption>,
    preferredMediaPlayer: String?,
    currentMedia: String,
    navigationStatus: String,
    syncStatus: String,
    pairingInProgress: Boolean,
    onPair: () -> Unit,
    onConnectDevice: (String) -> Unit,
    onForgetDevice: (KnownDevice) -> Unit,
    onGpsChanged: (Boolean) -> Unit,
    onEnableMedia: () -> Unit,
    onMediaPlayerSelected: (String?) -> Unit,
    repository: DeviceSettingsRepository,
) {
    val rideActive = telemetry.rideState == 1 || telemetry.rideState == 2
    val editable = ready && !rideActive
    Column(
        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionTitle("Connection", connectionStatus(connectionState))
        DeviceList(
            devices = devices,
            ready = ready,
            activeBluetoothAddress = activeBluetoothAddress,
            onPair = onPair,
            onConnect = onConnectDevice,
            onForget = onForgetDevice,
            showAddButton = true,
            addEnabled = !pairingInProgress,
        )

        SectionTitle("Device settings", if (ready) "Synchronized with the bike" else "Connect to edit")
        if (rideActive) {
            Text(
                "Finish the current ride before changing device settings.",
                color = MaterialTheme.colorScheme.tertiary,
            )
        }
        SettingsCard("Ride") {
            SettingStepper(
                "Wheel circumference",
                "%.3f m".format(settings.wheelCircumferenceM),
                editable,
                { repository.setWheelCircumference((settings.wheelCircumferenceM - 0.005f).coerceAtLeast(0.5f)) },
                { repository.setWheelCircumference((settings.wheelCircumferenceM + 0.005f).coerceAtMost(3.5f)) },
            )
            SettingStepper(
                "Stop threshold",
                "%.1f km/h".format(settings.stopThresholdKmh),
                editable,
                { repository.setStopThreshold((settings.stopThresholdKmh - 0.5f).coerceAtLeast(0.5f)) },
                { repository.setStopThreshold((settings.stopThresholdKmh + 0.5f).coerceAtMost(15f)) },
            )
            ToggleSetting(
                "Auto Pause",
                "Stops moving time while the wheel is still",
                settings.autoPauseEnabled,
                editable,
            ) { repository.setAutoPause(it) }
            SettingStepper(
                "Auto Pause delay",
                "%.1f s".format(settings.autoPauseDelayMs / 1000f),
                editable,
                { repository.setAutoPauseDelay((settings.autoPauseDelayMs - 500).coerceAtLeast(1_000)) },
                { repository.setAutoPauseDelay((settings.autoPauseDelayMs + 500).coerceAtMost(60_000)) },
            )
            SettingStepper(
                "Log interval",
                "%.2f s".format(settings.logIntervalMs / 1000f),
                editable,
                { repository.setLogInterval((settings.logIntervalMs - 250).coerceAtLeast(250)) },
                { repository.setLogInterval((settings.logIntervalMs + 250).coerceAtMost(10_000)) },
            )
            SettingStepper(
                "Graph window",
                "${settings.graphWindowSeconds} s",
                editable,
                { repository.setGraphWindow((settings.graphWindowSeconds - 10).coerceAtLeast(10)) },
                { repository.setGraphWindow((settings.graphWindowSeconds + 10).coerceAtMost(300)) },
            )
        }
        SettingsCard("Speed LED") {
            ToggleSetting(
                "Indicator",
                "GPIO48 follows the 2-second speed trend",
                settings.speedLedEnabled,
                editable,
            ) { repository.setSpeedLedEnabled(it) }
            SettingStepper(
                "Stable range · 2 s",
                "± %.1f km/h".format(settings.speedLedTolerance2sKmh),
                editable,
                { repository.setSpeedLedTolerance2s((settings.speedLedTolerance2sKmh - 0.1f).coerceAtLeast(0.1f)) },
                { repository.setSpeedLedTolerance2s((settings.speedLedTolerance2sKmh + 0.1f).coerceAtMost(5f)) },
            )
            SettingStepper(
                "Stable range · 5 s",
                "± %.1f km/h".format(settings.speedLedTolerance5sKmh),
                editable,
                { repository.setSpeedLedTolerance5s((settings.speedLedTolerance5sKmh - 0.1f).coerceAtLeast(0.1f)) },
                { repository.setSpeedLedTolerance5s((settings.speedLedTolerance5sKmh + 0.1f).coerceAtMost(5f)) },
            )
            SettingStepper(
                "Stable range · 10 s",
                "± %.1f km/h".format(settings.speedLedTolerance10sKmh),
                editable,
                { repository.setSpeedLedTolerance10s((settings.speedLedTolerance10sKmh - 0.1f).coerceAtLeast(0.1f)) },
                { repository.setSpeedLedTolerance10s((settings.speedLedTolerance10sKmh + 0.1f).coerceAtMost(5f)) },
            )
            SettingStepper(
                "LED brightness",
                "${settings.speedLedBrightnessPercent}%",
                editable,
                { repository.setSpeedLedBrightness((settings.speedLedBrightnessPercent - 5).coerceAtLeast(5)) },
                { repository.setSpeedLedBrightness((settings.speedLedBrightnessPercent + 5).coerceAtMost(100)) },
            )
        }
        settings.lastResult?.let {
            Text(it, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }

        SectionTitle("App settings", "Phone-side companion features")
        SettingsCard("Location") {
            ToggleSetting(
                "Send ride location",
                "Forwarded to the ESP; live fixes are not stored on the phone",
                gpsEnabled,
                true,
                onGpsChanged,
            )
            Text(
                gpsStatusMessage,
                color = if (gpsEnabled) {
                    MaterialTheme.colorScheme.primary
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
                fontSize = 12.sp,
            )
            Text(
                "The bike computer stores fresh fixes in its samples.csv. " +
                    "Wheel distance remains authoritative.",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 12.sp,
            )
        }
        SettingsCard("Music control") {
            Text(currentMedia, color = MaterialTheme.colorScheme.onSurfaceVariant)
            if (!mediaAccessEnabled) {
                Button(onClick = onEnableMedia) { Text("Grant media session access") }
            } else {
                Text("Control source", fontWeight = FontWeight.SemiBold)
                PlayerChoice(
                    label = "Auto · currently playing",
                    selected = preferredMediaPlayer == null,
                    onClick = { onMediaPlayerSelected(null) },
                )
                mediaPlayers.forEach { player ->
                    PlayerChoice(
                        label = buildString {
                            append(player.name)
                            if (player.playing) append(" · Playing")
                            else if (player.title.isNotBlank()) append(" · ${player.title}")
                        },
                        selected = preferredMediaPlayer == player.packageName,
                        onClick = { onMediaPlayerSelected(player.packageName) },
                    )
                }
                OutlinedButton(onClick = onEnableMedia) { Text("Manage media access") }
            }
        }
        SettingsCard("Device information") {
            ReadOnlyRow("Display", "ST7796 · 480×320")
            ReadOnlyRow("Touch", "FT6336")
            ReadOnlyRow("System", "ESP32-S3-N16R8")
            ReadOnlyRow("Firmware target", "2.2.0")
            ReadOnlyRow("Config values", "${settings.loadedKeys.size}/11 loaded")
            ReadOnlyRow("Ride sync", syncStatus)
            ReadOnlyRow("Navigation", navigationStatus)
        }
    }
}

@Composable
private fun DeviceList(
    devices: List<KnownDevice>,
    ready: Boolean,
    activeBluetoothAddress: String?,
    onPair: () -> Unit,
    onConnect: (String) -> Unit,
    onForget: (KnownDevice) -> Unit,
    showAddButton: Boolean,
    addEnabled: Boolean = true,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text("Remembered devices", fontWeight = FontWeight.Bold)
            if (devices.isEmpty()) {
                Text(
                    "No bike computers remembered yet.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            devices.forEach { device ->
                val isActive = ready &&
                    device.bluetoothAddress.equals(activeBluetoothAddress, ignoreCase = true)
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(device.displayName, fontWeight = FontWeight.SemiBold)
                        Text(
                            if (device.authorized) "Ready for secure reconnect" else "Pairing incomplete",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 12.sp,
                        )
                    }
                    TextButton(
                        onClick = { onConnect(device.bluetoothAddress) },
                        enabled = !isActive,
                    ) {
                        Text(if (isActive) "Active" else "Connect")
                    }
                    TextButton(onClick = { onForget(device) }) {
                        Text("Forget", color = MaterialTheme.colorScheme.error)
                    }
                }
            }
            if (showAddButton) {
                FilledTonalButton(onClick = onPair, enabled = addEnabled) {
                    Text("Add bike computer")
                }
            }
        }
    }
}

@Composable
private fun SettingsCard(title: String, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(title, fontSize = 18.sp, fontWeight = FontWeight.Bold)
            content()
        }
    }
}

@Composable
private fun SectionTitle(title: String, subtitle: String) {
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Text(title, fontSize = 20.sp, fontWeight = FontWeight.Bold)
        Text(subtitle, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
    }
}

@Composable
private fun ToggleSetting(
    label: String,
    description: String,
    checked: Boolean,
    enabled: Boolean,
    onCheckedChange: (Boolean) -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(label, fontWeight = FontWeight.SemiBold)
            Text(
                description,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 12.sp,
            )
        }
        Switch(checked = checked, onCheckedChange = onCheckedChange, enabled = enabled)
    }
}

@Composable
private fun PlayerChoice(label: String, selected: Boolean, onClick: () -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        RadioButton(selected = selected, onClick = onClick)
        Spacer(Modifier.width(4.dp))
        Text(label, modifier = Modifier.weight(1f))
    }
}

@Composable
private fun SettingStepper(
    label: String,
    value: String,
    enabled: Boolean,
    onDecrease: () -> Unit,
    onIncrease: () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 13.sp)
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedButton(onClick = onDecrease, enabled = enabled) { Text("−") }
            Text(
                value,
                modifier = Modifier.weight(1f),
                textAlign = androidx.compose.ui.text.style.TextAlign.Center,
                fontWeight = FontWeight.SemiBold,
            )
            OutlinedButton(onClick = onIncrease, enabled = enabled) { Text("+") }
        }
    }
}

@Composable
private fun ReadOnlyRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth()) {
        Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.weight(1f))
        Text(value, fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun MetricCard(label: String, value: String, modifier: Modifier) {
    Card(modifier = modifier) {
        Column(Modifier.padding(15.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 11.sp)
            Text(value, fontSize = 20.sp, fontWeight = FontWeight.Bold)
        }
    }
}

private fun duration(milliseconds: Long): String {
    val seconds = milliseconds / 1000
    return "%02d:%02d:%02d".format(seconds / 3600, (seconds / 60) % 60, seconds % 60)
}

private fun rideStateLabel(state: Int): String = when (state) {
    1 -> "Ride in progress"
    2 -> "Ride paused"
    3 -> "Ride finished"
    else -> "Ready to ride"
}

private fun storageStateLabel(state: Int): String = when (state) {
    1 -> "ready"
    2 -> "error"
    3 -> "USB host"
    else -> "missing"
}

private fun connectionStatus(state: BikeConnectionState): String = when (state) {
    BikeConnectionState.Unpaired -> "Not paired"
    BikeConnectionState.Connecting -> "Connecting over Bluetooth…"
    BikeConnectionState.Initializing -> "Securing link and loading data…"
    BikeConnectionState.Ready -> "Connected"
    BikeConnectionState.Reconnecting -> "Searching for the remembered device…"
    BikeConnectionState.Disconnected -> "Disconnected"
    BikeConnectionState.Error -> "Connection error"
}

@Composable
private fun connectionColor(state: BikeConnectionState): Color = when (state) {
    BikeConnectionState.Ready -> MaterialTheme.colorScheme.primary
    BikeConnectionState.Error -> MaterialTheme.colorScheme.error
    else -> MaterialTheme.colorScheme.tertiary
}

private fun connectionHint(state: BikeConnectionState, hasTarget: Boolean): String = when {
    !hasTarget -> "Add the bike computer and choose it in the Android system dialog."
    state == BikeConnectionState.Reconnecting ->
        "Keep the bike computer nearby and make sure Bluetooth is enabled."
    state == BikeConnectionState.Error ->
        "Try Connect again. If the bond was removed in Android settings, forget and pair again."
    else -> "The live dashboard will replace this connection view as soon as the link is ready."
}
