package com.diybikecomputer.companion.ui.ride

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import com.diybikecomputer.companion.rides.GpsPointEntity

@Composable
fun GpsRouteMap(points: List<GpsPointEntity>, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier.fillMaxWidth().height(220.dp)) {
        if (points.size < 2) return@Canvas
        val minimumLatitude = points.minOf { it.latitude }
        val maximumLatitude = points.maxOf { it.latitude }
        val minimumLongitude = points.minOf { it.longitude }
        val maximumLongitude = points.maxOf { it.longitude }
        val latitudeSpan = (maximumLatitude - minimumLatitude).coerceAtLeast(0.000001)
        val longitudeSpan = (maximumLongitude - minimumLongitude).coerceAtLeast(0.000001)
        val padding = 14f
        val route = Path()
        points.forEachIndexed { index, point ->
            val x = padding +
                ((point.longitude - minimumLongitude) / longitudeSpan).toFloat() *
                (size.width - padding * 2)
            val y = padding +
                (1f - ((point.latitude - minimumLatitude) / latitudeSpan).toFloat()) *
                (size.height - padding * 2)
            if (index == 0) route.moveTo(x, y) else route.lineTo(x, y)
        }
        drawPath(
            path = route,
            color = Color(0xFF55D7C4),
            style = Stroke(width = 5f, cap = StrokeCap.Round),
        )
        val first = points.first()
        val last = points.last()
        fun pointOffset(point: GpsPointEntity) = Offset(
            x = padding +
                ((point.longitude - minimumLongitude) / longitudeSpan).toFloat() *
                (size.width - padding * 2),
            y = padding +
                (1f - ((point.latitude - minimumLatitude) / latitudeSpan).toFloat()) *
                (size.height - padding * 2),
        )
        drawCircle(Color(0xFF66D19E), 7f, pointOffset(first))
        drawCircle(Color(0xFFF36B62), 7f, pointOffset(last))
    }
}
