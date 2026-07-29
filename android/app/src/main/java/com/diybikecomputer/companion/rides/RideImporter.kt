package com.diybikecomputer.companion.rides

import androidx.room.withTransaction
import java.io.File
import org.json.JSONObject

class RideImporter(private val database: RideDatabase) {
    suspend fun importVerifiedFiles(rideId: String, files: List<RideFileEntity>) {
        val samplesFile = files.firstOrNull { it.fileId == 2 && it.verified }
            ?.verifiedPath?.let(::File)
        val eventsFile = files.firstOrNull { it.fileId == 3 && it.verified }
            ?.verifiedPath?.let(::File)
        val summaryFile = files.firstOrNull { it.fileId == 4 && it.verified }
            ?.verifiedPath?.let(::File)
        database.withTransaction {
            val dao = database.rideDao()
            if (summaryFile?.isFile == true) {
                runCatching { JSONObject(summaryFile.readText()) }.getOrNull()?.let { summary ->
                    dao.getRide(rideId)?.let { ride ->
                        dao.upsertRide(
                            ride.copy(
                                startedAtUtcMs = summary.optionalLong("started_at_utc_ms")
                                    ?: ride.startedAtUtcMs,
                                finishedAtUtcMs = summary.optionalLong("finished_at_utc_ms")
                                    ?: ride.finishedAtUtcMs,
                                distanceM = summary.optDouble("distance_m", ride.distanceM),
                                movingTimeMs =
                                    summary.optLong("moving_time_ms", ride.movingTimeMs),
                                elapsedTimeMs =
                                    summary.optLong("elapsed_time_ms", ride.elapsedTimeMs),
                                averageSpeedKmh = summary.optDouble(
                                    "average_moving_speed_kmh",
                                    ride.averageSpeedKmh,
                                ),
                                maxSpeedKmh =
                                    summary.optDouble("max_speed_kmh", ride.maxSpeedKmh),
                            ),
                        )
                    }
                }
            }
            if (samplesFile?.isFile == true) {
                dao.deleteSamples(rideId)
                samplesFile.useLines { lines ->
                    val batch = ArrayList<RideSampleEntity>(250)
                    lines.drop(1).forEach { line ->
                        val columns = csvColumns(line)
                        if (columns.size < 13) return@forEach
                        batch += RideSampleEntity(
                            rideId = rideId,
                            sampleIndex = columns[0].toLongOrNull() ?: return@forEach,
                            elapsedMs = columns[1].toLongOrNull() ?: return@forEach,
                            speedKmh = columns[3].toDoubleOrNull() ?: 0.0,
                            distanceM = columns[5].toDoubleOrNull() ?: 0.0,
                            pulseCount = columns[12].toLongOrNull() ?: 0,
                        )
                        if (batch.size == 250) {
                            dao.upsertSamples(batch.toList())
                            batch.clear()
                        }
                    }
                    if (batch.isNotEmpty()) dao.upsertSamples(batch)
                }
            }
            if (eventsFile?.isFile == true) {
                dao.deleteEvents(rideId)
                val events = ArrayList<RideEventEntity>()
                eventsFile.useLines { lines ->
                    lines.drop(1).forEachIndexed { index, line ->
                        val columns = csvColumns(line)
                        if (columns.size < 2) return@forEachIndexed
                        events += RideEventEntity(
                            rideId = rideId,
                            eventIndex = index.toLong(),
                            elapsedMs = columns[0].toLongOrNull() ?: 0,
                            type = columns[1],
                            detail = columns.getOrNull(2),
                        )
                    }
                }
                if (events.isNotEmpty()) dao.upsertEvents(events)
            }
        }
    }

    private fun csvColumns(line: String): List<String> {
        val result = ArrayList<String>()
        val value = StringBuilder()
        var quoted = false
        var index = 0
        while (index < line.length) {
            val character = line[index]
            when {
                character == '"' && quoted && index + 1 < line.length &&
                    line[index + 1] == '"' -> {
                    value.append('"')
                    index++
                }
                character == '"' -> quoted = !quoted
                character == ',' && !quoted -> {
                    result += value.toString()
                    value.clear()
                }
                else -> value.append(character)
            }
            index++
        }
        result += value.toString()
        return result
    }

    private fun JSONObject.optionalLong(name: String): Long? =
        if (has(name) && !isNull(name)) optLong(name) else null
}
