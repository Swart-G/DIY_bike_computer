package com.diybikecomputer.companion.rides

import java.io.File

/** Reads device-owned format-v2 coordinates without inserting them into Room. */
object RideGpsFileReader {
    suspend fun read(dao: RideDao, rideId: String): List<GpsPointEntity> {
        val rideFormat = dao.getRide(rideId)?.formatVersion ?: 1
        val source = dao.getFiles(rideId)
            .firstOrNull { it.fileId == 2 && it.verified }
            ?.verifiedPath?.let(::File)
            ?.takeIf(File::isFile)
        val fromDevice = source?.let(::readFile).orEmpty()
        // Keep old v1 history usable. New live fixes are never inserted into this table.
        return if (rideFormat >= 2 || fromDevice.isNotEmpty()) {
            fromDevice
        } else {
            dao.getGpsPoints(rideId)
        }
    }

    fun readFile(source: File): List<GpsPointEntity> {
        val points = ArrayList<GpsPointEntity>()
        source.useLines { lines ->
            val iterator = lines.iterator()
            if (!iterator.hasNext()) return@useLines
            val header = csvColumns(iterator.next())
            val timestampIndex = header.indexOf("timestamp_utc_ms")
            val latitudeIndex = header.indexOf("latitude")
            val longitudeIndex = header.indexOf("longitude")
            if (timestampIndex < 0 || latitudeIndex < 0 || longitudeIndex < 0) {
                return@useLines
            }
            val altitudeIndex = header.indexOf("altitude_m")
            val accuracyIndex = header.indexOf("gps_accuracy_m")
            val speedIndex = header.indexOf("gps_speed_mps")
            var lastTimestamp = Long.MIN_VALUE
            while (iterator.hasNext()) {
                val columns = csvColumns(iterator.next())
                val timestamp = columns.getOrNull(timestampIndex)?.toLongOrNull()
                    ?.takeIf { it > 0 && it != lastTimestamp } ?: continue
                val latitude = columns.getOrNull(latitudeIndex)?.toDoubleOrNull()
                    ?.takeIf { it.isFinite() && it in -90.0..90.0 } ?: continue
                val longitude = columns.getOrNull(longitudeIndex)?.toDoubleOrNull()
                    ?.takeIf { it.isFinite() && it in -180.0..180.0 } ?: continue
                lastTimestamp = timestamp
                points += GpsPointEntity(
                    rideId = "",
                    pointIndex = points.size.toLong(),
                    timestampUtcMs = timestamp,
                    latitude = latitude,
                    longitude = longitude,
                    altitudeM = columns.optionalFiniteDouble(altitudeIndex),
                    accuracyM = columns.optionalFiniteFloat(accuracyIndex)
                        ?.takeIf { it >= 0f },
                    diagnosticGpsSpeedMps = columns.optionalFiniteFloat(speedIndex)
                        ?.takeIf { it >= 0f },
                )
            }
        }
        return points
    }

    private fun List<String>.optionalFiniteDouble(index: Int): Double? =
        getOrNull(index)?.toDoubleOrNull()?.takeIf(Double::isFinite)

    private fun List<String>.optionalFiniteFloat(index: Int): Float? =
        getOrNull(index)?.toFloatOrNull()?.takeIf(Float::isFinite)

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
}
