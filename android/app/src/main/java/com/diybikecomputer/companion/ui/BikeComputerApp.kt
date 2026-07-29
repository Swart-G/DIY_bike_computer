package com.diybikecomputer.companion.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.DirectionsBike
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.MonitorHeart
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.ble.BikeConnectionState
import com.diybikecomputer.companion.media.MediaRepository
import com.diybikecomputer.companion.navigation.NavigationRepository
import com.diybikecomputer.companion.rides.RideDatabase
import com.diybikecomputer.companion.rides.RideSyncManager
import com.diybikecomputer.companion.settings.DeviceSettingsRepository
import com.diybikecomputer.companion.ui.history.HistoryScreen
import com.diybikecomputer.companion.ui.history.RideDetailScreen

private enum class AppPage(val label: String) {
    Home("Home"),
    History("History"),
    Device("Bike"),
    Diagnostics("Status"),
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
    mediaAccessEnabled: Boolean,
    pairingInProgress: Boolean,
    pairingMessage: String?,
    onPair: () -> Unit,
    onEnableGps: () -> Unit,
    onEnableMedia: () -> Unit,
    onExportCsv: (String) -> Unit,
    onExportGpx: (String) -> Unit,
) {
    val connectionState by connection.state.collectAsStateWithLifecycle()
    val telemetry by connection.telemetry.collectAsStateWithLifecycle()
    val media by mediaRepository.state.collectAsStateWithLifecycle()
    val navigation by navigationRepository.state.collectAsStateWithLifecycle()
    val deviceSettings by deviceSettingsRepository.state.collectAsStateWithLifecycle()
    val sync by rideSync.progress.collectAsStateWithLifecycle()
    val ready = connectionState == BikeConnectionState.Ready
    val canPair =
        connectionState == BikeConnectionState.Unpaired ||
            connectionState == BikeConnectionState.Error
    var page by remember { mutableStateOf(AppPage.Home) }
    var selectedRide by remember { mutableStateOf<String?>(null) }

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
                                    AppPage.Device -> Icons.AutoMirrored.Rounded.DirectionsBike
                                    AppPage.Diagnostics -> Icons.Rounded.MonitorHeart
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
            Text("Bike Computer", fontSize = 27.sp, fontWeight = FontWeight.Bold)
            when {
                selectedRide != null -> RideDetailScreen(
                    dao = database.rideDao(),
                    rideId = selectedRide!!,
                    onBack = { selectedRide = null },
                    onExportCsv = onExportCsv,
                    onExportGpx = onExportGpx,
                )
                page == AppPage.Home -> HomePage(
                    connectionState.name,
                    ready,
                    canPair,
                    telemetry.speedKmh,
                    telemetry.distanceM,
                    telemetry.batteryPercent,
                    telemetry.pulseCount,
                    pairingInProgress,
                    pairingMessage,
                    onPair,
                )
                page == AppPage.History -> HistoryScreen(database.rideDao()) {
                    selectedRide = it
                }
                page == AppPage.Device -> DevicePage(
                    ready,
                    gpsEnabled,
                    mediaAccessEnabled,
                    media.available,
                    media.playing,
                    media.title,
                    navigation.available,
                    navigation.lifecycle.name,
                    navigation.streetName,
                    deviceSettings.wheelCircumferenceM,
                    deviceSettings.stopThresholdKmh,
                    deviceSettings.autoPauseEnabled,
                    deviceSettings.autoPauseDelayMs,
                    deviceSettings.logIntervalMs,
                    deviceSettings.graphWindowSeconds,
                    deviceSettings.lastResult,
                    onEnableGps,
                    onEnableMedia,
                    { deviceSettingsRepository.setAutoPause(!deviceSettings.autoPauseEnabled) },
                    { deviceSettingsRepository.setWheelCircumference(it) },
                    { deviceSettingsRepository.setStopThreshold(it) },
                    { deviceSettingsRepository.setAutoPauseDelay(it) },
                    { deviceSettingsRepository.setLogInterval(it) },
                    { deviceSettingsRepository.setGraphWindow(it) },
                )
                else -> DiagnosticsPage(
                    connectionState.name,
                    telemetry.rideState,
                    telemetry.sdState,
                    telemetry.motionState,
                    sync.state.name,
                    sync.downloadedBytes,
                    sync.totalBytes,
                )
            }
        }
    }
}

@Composable
private fun HomePage(
    connectionState: String,
    ready: Boolean,
    canPair: Boolean,
    speedKmh: Float,
    distanceM: Float,
    batteryPercent: Int,
    pulseCount: Long,
    pairingInProgress: Boolean,
    pairingMessage: String?,
    onPair: () -> Unit,
) {
    Column(
        modifier = Modifier.verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Text(
            "Autonomous computer + optional companion",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Device", fontWeight = FontWeight.Bold)
                Text(connectionState)
                Button(onClick = onPair, enabled = canPair && !pairingInProgress) {
                    Text(
                        when {
                            ready -> "Connected"
                            pairingInProgress -> "Pairing…"
                            else -> "Pair device"
                        },
                    )
                }
                pairingMessage?.let {
                    Text(it, color = MaterialTheme.colorScheme.error)
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            StatusCard("SPEED", "%.1f km/h".format(speedKmh), Modifier.weight(1f))
            StatusCard("DISTANCE", "%.2f km".format(distanceM / 1000f), Modifier.weight(1f))
        }
        Text(
            "ESP authority · $batteryPercent% battery · $pulseCount pulses",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun DevicePage(
    ready: Boolean,
    gpsEnabled: Boolean,
    mediaAccessEnabled: Boolean,
    mediaAvailable: Boolean,
    mediaPlaying: Boolean,
    mediaTitle: String,
    navigationAvailable: Boolean,
    navigationLifecycle: String,
    navigationStreet: String,
    wheelM: Float,
    thresholdKmh: Float,
    autoPause: Boolean,
    autoPauseDelayMs: Long,
    logIntervalMs: Long,
    graphWindowSeconds: Long,
    lastResult: String?,
    onEnableGps: () -> Unit,
    onEnableMedia: () -> Unit,
    onToggleAutoPause: () -> Unit,
    onWheelChanged: (Float) -> Unit,
    onThresholdChanged: (Float) -> Unit,
    onAutoPauseDelayChanged: (Long) -> Unit,
    onLogIntervalChanged: (Long) -> Unit,
    onGraphWindowChanged: (Long) -> Unit,
) {
    Column(
        modifier = Modifier.verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Button(onClick = onEnableGps, enabled = !gpsEnabled) {
            Text(if (gpsEnabled) "GPS Assist enabled" else "Enable GPS Assist")
        }
        Button(onClick = onEnableMedia, enabled = !mediaAccessEnabled) {
            Text(
                when {
                    !mediaAccessEnabled -> "Set up media controls"
                    mediaAvailable -> "${if (mediaPlaying) "Playing" else "Paused"} · $mediaTitle"
                    else -> "Media access enabled · Nothing playing"
                },
            )
        }
        Card(modifier = Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Device settings", fontWeight = FontWeight.Bold)
                SettingStepper(
                    "Wheel circumference",
                    "%.3f m".format(wheelM),
                    ready,
                    { onWheelChanged((wheelM - 0.005f).coerceAtLeast(0.5f)) },
                    { onWheelChanged((wheelM + 0.005f).coerceAtMost(3.5f)) },
                )
                SettingStepper(
                    "Stop threshold",
                    "%.1f km/h".format(thresholdKmh),
                    ready,
                    { onThresholdChanged((thresholdKmh - 0.5f).coerceAtLeast(0.5f)) },
                    { onThresholdChanged((thresholdKmh + 0.5f).coerceAtMost(15f)) },
                )
                Button(onClick = onToggleAutoPause, enabled = ready) {
                    Text(
                        if (autoPause) "Auto Pause on · ${autoPauseDelayMs / 1000f} s"
                        else "Auto Pause off",
                    )
                }
                SettingStepper(
                    "Auto Pause delay",
                    "%.0f s".format(autoPauseDelayMs / 1000f),
                    ready,
                    { onAutoPauseDelayChanged((autoPauseDelayMs - 1_000).coerceAtLeast(1_000)) },
                    { onAutoPauseDelayChanged((autoPauseDelayMs + 1_000).coerceAtMost(60_000)) },
                )
                SettingStepper(
                    "Log interval",
                    "$logIntervalMs ms",
                    ready,
                    { onLogIntervalChanged((logIntervalMs - 250).coerceAtLeast(250)) },
                    { onLogIntervalChanged((logIntervalMs + 250).coerceAtMost(10_000)) },
                )
                SettingStepper(
                    "Graph window",
                    "$graphWindowSeconds s",
                    ready,
                    { onGraphWindowChanged((graphWindowSeconds - 10).coerceAtLeast(10)) },
                    { onGraphWindowChanged((graphWindowSeconds + 10).coerceAtMost(300)) },
                )
                lastResult?.let {
                    Text(it, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
        Text(
            if (navigationAvailable) {
                "Navigation · $navigationLifecycle · $navigationStreet"
            } else {
                "Navigation · Experimental · Provider unavailable"
            },
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
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
    Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Button(onClick = onDecrease, enabled = enabled) { Text("−") }
        Text(value, modifier = Modifier.padding(vertical = 12.dp))
        Button(onClick = onIncrease, enabled = enabled) { Text("+") }
    }
}

@Composable
private fun DiagnosticsPage(
    connection: String,
    rideState: Int,
    sdState: Int,
    motionState: Int,
    syncState: String,
    syncedBytes: Long,
    totalBytes: Long,
) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("Diagnostics", fontWeight = FontWeight.Bold)
            Text("BLE · $connection")
            Text("Ride state · $rideState")
            Text("Motion · ${if (motionState == 1) "AUTO_PAUSED" else "MOVING"}")
            Text("Storage · $sdState")
            Text("Sync · $syncState · $syncedBytes / $totalBytes bytes")
            Text("Protocol · 1 · Firmware target 2.0.0-dev")
        }
    }
}

@Composable
private fun StatusCard(label: String, value: String, modifier: Modifier) {
    Card(modifier = modifier) {
        Column(Modifier.padding(15.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Text(label, color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 12.sp)
            Text(value, fontSize = 21.sp, fontWeight = FontWeight.Bold)
        }
    }
}
