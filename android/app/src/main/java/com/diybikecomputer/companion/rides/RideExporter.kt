package com.diybikecomputer.companion.rides

import android.content.ContentResolver
import android.net.Uri
import java.io.File
import java.io.OutputStreamWriter
import kotlin.math.abs

class RideExporter(
    private val resolver: ContentResolver,
    private val database: RideDatabase,
) {
    suspend fun exportCsv(rideId: String, destination: Uri): Boolean {
        val dao = database.rideDao()
        val source = dao.getFiles(rideId)
            .firstOrNull { it.fileId == 2 && it.verified }
            ?.verifiedPath?.let(::File)
            ?.takeIf(File::isFile) ?: return false
        val ride = dao.getRide(rideId) ?: return false
        val points = dao.getGpsPoints(rideId)
        val firstSampleElapsed = source.useLines { lines ->
            lines.drop(1).firstOrNull()?.let(::csvColumns)?.getOrNull(1)?.toLongOrNull()
        } ?: 0L
        val inferredStartUtcMs = ride.startedAtUtcMs
            ?: points.firstOrNull()?.let { it.timestampUtcMs - firstSampleElapsed }

        resolver.openOutputStream(destination, "wt")?.use { stream ->
            OutputStreamWriter(stream, Charsets.UTF_8).buffered().use { writer ->
                source.useLines { lines ->
                    val iterator = lines.iterator()
                    if (!iterator.hasNext()) return@useLines
                    writer.append(iterator.next())
                    writer.appendLine(
                        ",timestamp_utc_ms,latitude,longitude,altitude_m," +
                            "gps_accuracy_m,gps_speed_mps",
                    )
                    var pointIndex = 0
                    while (iterator.hasNext()) {
                        val line = iterator.next()
                        val elapsedMs = csvColumns(line).getOrNull(1)?.toLongOrNull()
                        val targetUtcMs =
                            if (elapsedMs != null && inferredStartUtcMs != null) {
                                inferredStartUtcMs + elapsedMs
                            } else {
                                null
                            }
                        if (targetUtcMs != null && points.isNotEmpty()) {
                            while (pointIndex + 1 < points.size &&
                                abs(points[pointIndex + 1].timestampUtcMs - targetUtcMs) <=
                                abs(points[pointIndex].timestampUtcMs - targetUtcMs)
                            ) {
                                pointIndex++
                            }
                        }
                        val point = targetUtcMs?.let {
                            points.getOrNull(pointIndex)
                                ?.takeIf { candidate ->
                                    abs(candidate.timestampUtcMs - targetUtcMs) <=
                                        MAX_GPS_MATCH_GAP_MS
                                }
                        }
                        writer.append(line)
                        writer.append(',')
                        if (point != null) {
                            writer.append(point.timestampUtcMs.toString()).append(',')
                            writer.append(point.latitude.toString()).append(',')
                            writer.append(point.longitude.toString()).append(',')
                            writer.append(point.altitudeM?.toString().orEmpty()).append(',')
                            writer.append(point.accuracyM?.toString().orEmpty()).append(',')
                            writer.append(point.diagnosticGpsSpeedMps?.toString().orEmpty())
                        } else {
                            writer.append(",,,,,")
                        }
                        writer.newLine()
                    }
                }
            }
        } ?: return false
        return true
    }

    suspend fun exportSummaryXlsx(rideId: String, destination: Uri): Boolean {
        val dao = database.rideDao()
        val ride = dao.getRide(rideId) ?: return false
        val gpsPointCount = dao.getGpsPoints(rideId).size
        resolver.openOutputStream(destination, "wt")?.use { output ->
            RideSummaryXlsx.write(output, ride, gpsPointCount)
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

    private fun csvColumns(line: String): List<String> {
        val result = ArrayList<String>()
        val value = StringBuilder()
        var quoted = false
        var index = 0
        while (index < line.length) {
            when {
                line[index] == '"' && quoted && index + 1 < line.length &&
                    line[index + 1] == '"' -> {
                    value.append('"')
                    index++
                }
                line[index] == '"' -> quoted = !quoted
                line[index] == ',' && !quoted -> {
                    result += value.toString()
                    value.clear()
                }
                else -> value.append(line[index])
            }
            index++
        }
        result += value.toString()
        return result
    }

    companion object {
        private const val MAX_GPS_MATCH_GAP_MS = 5_000L
    }
}
