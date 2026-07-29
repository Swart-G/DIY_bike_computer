package com.diybikecomputer.companion.rides

import android.content.ContentResolver
import android.net.Uri
import java.io.OutputStreamWriter

class RideExporter(
    private val resolver: ContentResolver,
    private val database: RideDatabase,
) {
    suspend fun exportCsv(rideId: String, destination: Uri): Boolean {
        val samples = database.rideDao().getFiles(rideId)
            .firstOrNull { it.fileId == 2 && it.verified }
            ?.verifiedPath ?: return false
        resolver.openOutputStream(destination, "wt")?.use { output ->
            java.io.File(samples).inputStream().use { it.copyTo(output) }
        } ?: return false
        return true
    }

    suspend fun exportGpx(rideId: String, destination: Uri): Boolean {
        val points = database.rideDao().getGpsPoints(rideId)
        if (points.isEmpty()) return false
        resolver.openOutputStream(destination, "wt")?.use { stream ->
            OutputStreamWriter(stream, Charsets.UTF_8).use { writer ->
                writer.appendLine("""<?xml version="1.0" encoding="UTF-8"?>""")
                writer.appendLine(
                    """<gpx version="1.1" creator="DIY Bike Computer" xmlns="http://www.topografix.com/GPX/1/1">""",
                )
                writer.appendLine("<trk><name>Bike ride</name><trkseg>")
                points.forEach { point ->
                    writer.append("""<trkpt lat="${point.latitude}" lon="${point.longitude}">""")
                    point.altitudeM?.let { writer.append("<ele>$it</ele>") }
                    writer.append("<time>${java.time.Instant.ofEpochMilli(point.timestampUtcMs)}</time>")
                    writer.appendLine("</trkpt>")
                }
                writer.appendLine("</trkseg></trk></gpx>")
            }
        } ?: return false
        return true
    }
}
