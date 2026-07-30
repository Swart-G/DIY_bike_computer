package com.diybikecomputer.companion.ui.history

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.diybikecomputer.companion.rides.RideDao
import com.diybikecomputer.companion.rides.RideEntity
import com.diybikecomputer.companion.rides.RideSampleEntity
import com.diybikecomputer.companion.ui.ride.GpsRouteMap
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

private val RideAccent = Color(0xFF55D7C4)

@Composable
fun HistoryScreen(dao: RideDao, onRide: (String) -> Unit) {
    val rides by dao.observeRides().collectAsStateWithLifecycle(initialValue = emptyList())
    var filter by remember { mutableStateOf(HistoryFilter.All) }
    val visibleRides = when (filter) {
        HistoryFilter.All -> rides
        HistoryFilter.Verified -> rides.filter(RideEntity::synced)
        HistoryFilter.NeedsSync -> rides.filterNot(RideEntity::synced)
    }

    if (rides.isEmpty()) {
        EmptyHistory()
        return
    }

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            HistoryOverview(rides)
        }
        item {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                HistoryFilter.entries.forEach { item ->
                    FilterChip(
                        selected = filter == item,
                        onClick = { filter = item },
                        label = { Text(item.label) },
                    )
                }
            }
        }
        if (visibleRides.isEmpty()) {
            item {
                Text(
                    "No rides in this filter",
                    modifier = Modifier.padding(vertical = 18.dp),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        items(visibleRides, key = { it.rideId }) { ride ->
            RideCard(ride = ride, onClick = { onRide(ride.rideId) })
        }
        item { Spacer(Modifier.height(4.dp)) }
    }
}

@Composable
private fun EmptyHistory() {
    Card(
        modifier = Modifier.fillMaxWidth().padding(top = 12.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant,
        ),
        shape = RoundedCornerShape(22.dp),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(28.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("Your rides will appear here", fontSize = 20.sp, fontWeight = FontWeight.Bold)
            Text(
                "Finish a ride and synchronize the bike computer to see its statistics.",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun HistoryOverview(rides: List<RideEntity>) {
    val totalDistanceKm = rides.sumOf { it.distanceM } / 1000.0
    val totalMovingMs = rides.sumOf { it.movingTimeMs }

    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Text("Ride history", fontSize = 22.sp, fontWeight = FontWeight.Bold)
        Text(
            "All synchronized activity at a glance",
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(22.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.primaryContainer,
            ),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(horizontal = 18.dp, vertical = 20.dp),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                OverviewMetric(
                    label = "DISTANCE",
                    value = "%.1f km".format(totalDistanceKm),
                    modifier = Modifier.weight(1.25f),
                )
                OverviewMetric(
                    label = "RIDES",
                    value = rides.size.toString(),
                    modifier = Modifier.weight(0.75f),
                )
                OverviewMetric(
                    label = "MOVING",
                    value = compactDuration(totalMovingMs),
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}

@Composable
private fun OverviewMetric(label: String, value: String, modifier: Modifier = Modifier) {
    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(
            label,
            color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.7f),
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold,
        )
        Text(
            value,
            color = MaterialTheme.colorScheme.onPrimaryContainer,
            fontSize = 19.sp,
            fontWeight = FontWeight.Bold,
            maxLines = 1,
        )
    }
}

@Composable
private fun RideCard(ride: RideEntity, onClick: () -> Unit) {
    ElevatedCard(
        modifier = Modifier.fillMaxWidth().clickable(onClick = onClick),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.elevatedCardColors(
            containerColor = MaterialTheme.colorScheme.surface,
        ),
    ) {
        Column(
            modifier = Modifier.padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    rideTitle(ride),
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.weight(1f),
                )
                SyncPill(ride.synced)
            }
            Row(verticalAlignment = Alignment.Bottom) {
                Text(
                    "%.2f".format(ride.distanceM / 1000.0),
                    fontSize = 30.sp,
                    fontWeight = FontWeight.Bold,
                )
                Text(
                    " km",
                    modifier = Modifier.padding(bottom = 4.dp),
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                CompactMetric("AVG", "%.1f km/h".format(ride.averageSpeedKmh), Modifier.weight(1f))
                CompactMetric("MAX", "%.1f km/h".format(ride.maxSpeedKmh), Modifier.weight(1f))
                CompactMetric("TIME", compactDuration(ride.movingTimeMs), Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun SyncPill(synced: Boolean) {
    val foreground = if (synced) RideAccent else MaterialTheme.colorScheme.tertiary
    Surface(
        color = foreground.copy(alpha = 0.14f),
        contentColor = foreground,
        shape = CircleShape,
    ) {
        Text(
            if (synced) "Verified" else "Syncing",
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp),
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold,
        )
    }
}

@Composable
private fun CompactMetric(label: String, value: String, modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .background(
                MaterialTheme.colorScheme.surfaceVariant,
                RoundedCornerShape(12.dp),
            )
            .padding(horizontal = 10.dp, vertical = 9.dp),
        verticalArrangement = Arrangement.spacedBy(3.dp),
    ) {
        Text(
            label,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 10.sp,
            fontWeight = FontWeight.Bold,
        )
        Text(value, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
    }
}

@Composable
fun RideDetailScreen(
    dao: RideDao,
    rideId: String,
    onBack: () -> Unit,
    onExportCsv: (String) -> Unit,
    onExportXlsx: (String) -> Unit,
    onExportGpx: (String) -> Unit,
) {
    val ride by dao.observeRide(rideId).collectAsStateWithLifecycle(initialValue = null)
    val samples by dao.observeSamples(rideId).collectAsStateWithLifecycle(initialValue = emptyList())
    val points by dao.observeGpsPoints(rideId).collectAsStateWithLifecycle(initialValue = emptyList())

    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        item {
            TextButton(onClick = onBack) {
                Text("←  Back to history")
            }
        }
        ride?.let { currentRide ->
            item { RideHero(currentRide) }
            item { RideMetricGrid(currentRide) }
        }
        if (samples.size >= 2) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(20.dp),
                ) {
                    Column(
                        modifier = Modifier.padding(18.dp),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Speed profile", fontSize = 18.sp, fontWeight = FontWeight.Bold)
                            Spacer(Modifier.weight(1f))
                            Text(
                                "%.1f km/h max".format(samples.maxOf { it.speedKmh }),
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                fontSize = 12.sp,
                            )
                        }
                        Text(
                            "Speed over the full ride",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 12.sp,
                        )
                        SpeedChart(samples)
                    }
                }
            }
        }
        if (points.size >= 2) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(20.dp),
                ) {
                    Column(
                        modifier = Modifier.padding(18.dp),
                        verticalArrangement = Arrangement.spacedBy(10.dp),
                    ) {
                        Text("GPS route", fontSize = 18.sp, fontWeight = FontWeight.Bold)
                        Text(
                            "${points.size} recorded points",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 12.sp,
                        )
                        GpsRouteMap(points, Modifier.fillMaxWidth())
                    }
                }
            }
        }
        item {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                FilledTonalButton(
                    onClick = { onExportCsv(rideId) },
                    enabled = ride?.synced == true,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("Full CSV · telemetry + location")
                }
                OutlinedButton(
                    onClick = { onExportXlsx(rideId) },
                    enabled = ride != null,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("Brief XLSX · ride summary")
                }
                Button(
                    onClick = { onExportGpx(rideId) },
                    enabled = points.isNotEmpty(),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Text("GPX · route")
                }
            }
        }
        item { Spacer(Modifier.height(4.dp)) }
    }
}

private enum class HistoryFilter(val label: String) {
    All("All"),
    Verified("Verified"),
    NeedsSync("Needs sync"),
}

@Composable
private fun RideHero(ride: RideEntity) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.cardColors(containerColor = Color.Transparent),
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .background(
                    Brush.linearGradient(
                        listOf(
                            MaterialTheme.colorScheme.primaryContainer,
                            MaterialTheme.colorScheme.surfaceVariant,
                        ),
                    ),
                )
                .padding(22.dp),
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        rideTitle(ride),
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                        fontWeight = FontWeight.SemiBold,
                        modifier = Modifier.weight(1f),
                    )
                    SyncPill(ride.synced)
                }
                Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                    Text(
                        "%.2f km".format(ride.distanceM / 1000.0),
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                        fontSize = 40.sp,
                        fontWeight = FontWeight.Bold,
                    )
                    Text(
                        if (ride.finished) "Completed ride" else "Recovered ride",
                        color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.7f),
                    )
                }
            }
        }
    }
}

@Composable
private fun RideMetricGrid(ride: RideEntity) {
    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            MetricTile(
                label = "AVERAGE SPEED",
                value = "%.1f".format(ride.averageSpeedKmh),
                unit = "km/h",
                modifier = Modifier.weight(1f),
            )
            MetricTile(
                label = "MAXIMUM SPEED",
                value = "%.1f".format(ride.maxSpeedKmh),
                unit = "km/h",
                modifier = Modifier.weight(1f),
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            MetricTile(
                label = "MOVING TIME",
                value = duration(ride.movingTimeMs),
                modifier = Modifier.weight(1f),
            )
            MetricTile(
                label = "ELAPSED TIME",
                value = duration(ride.elapsedTimeMs),
                modifier = Modifier.weight(1f),
            )
        }
    }
}

@Composable
private fun MetricTile(
    label: String,
    value: String,
    unit: String = "",
    modifier: Modifier = Modifier,
) {
    Card(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant,
        ),
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(
                label,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold,
            )
            Row(verticalAlignment = Alignment.Bottom) {
                Text(value, fontSize = 21.sp, fontWeight = FontWeight.Bold, maxLines = 1)
                if (unit.isNotEmpty()) {
                    Spacer(Modifier.width(4.dp))
                    Text(
                        unit,
                        modifier = Modifier.padding(bottom = 2.dp),
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 11.sp,
                    )
                }
            }
        }
    }
}

@Composable
private fun SpeedChart(samples: List<RideSampleEntity>) {
    val gridColor = MaterialTheme.colorScheme.outlineVariant
    val accent = MaterialTheme.colorScheme.primary
    Canvas(modifier = Modifier.fillMaxWidth().height(190.dp).padding(top = 12.dp)) {
        val graphTop = 8f
        val graphBottom = size.height - 8f
        val graphHeight = graphBottom - graphTop
        val maximum = samples.maxOf { it.speedKmh }.coerceAtLeast(10.0)

        for (step in 0..3) {
            val y = graphTop + graphHeight * step / 3f
            drawLine(
                color = gridColor,
                start = Offset(0f, y),
                end = Offset(size.width, y),
                strokeWidth = 1f,
            )
        }

        val line = Path()
        samples.forEachIndexed { index, sample ->
            val x = index.toFloat() * size.width / (samples.size - 1)
            val y = graphBottom - (sample.speedKmh / maximum).toFloat() * graphHeight
            if (index == 0) line.moveTo(x, y) else line.lineTo(x, y)
        }

        val fill = Path().apply {
            addPath(line)
            lineTo(size.width, graphBottom)
            lineTo(0f, graphBottom)
            close()
        }
        drawPath(fill, accent.copy(alpha = 0.14f))
        drawPath(line, accent, style = Stroke(width = 5f))
    }
}

private fun rideTitle(ride: RideEntity): String =
    ride.startedAtUtcMs?.let {
        DateTimeFormatter.ofPattern("EEE, d MMM · HH:mm")
            .withZone(ZoneId.systemDefault())
            .format(Instant.ofEpochMilli(it))
    } ?: "Ride ${ride.rideId.substringAfterLast(':')}"

private fun compactDuration(milliseconds: Long): String {
    val minutes = milliseconds / 60_000
    val hours = minutes / 60
    return if (hours > 0) "${hours}h ${minutes % 60}m" else "${minutes}m"
}

private fun duration(milliseconds: Long): String {
    val seconds = milliseconds / 1000
    return "%02d:%02d:%02d".format(seconds / 3600, (seconds / 60) % 60, seconds % 60)
}
