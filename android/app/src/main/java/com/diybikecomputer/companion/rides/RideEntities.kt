package com.diybikecomputer.companion.rides

import androidx.room.Entity
import androidx.room.ForeignKey
import androidx.room.Index

@Entity(tableName = "devices", primaryKeys = ["deviceId"])
data class DeviceEntity(
    val deviceId: Long,
    val associationId: Long,
    val displayName: String,
    val lastSeenUtcMs: Long,
    val protocolVersion: Int,
)

@Entity(
    tableName = "rides",
    indices = [Index("deviceId")],
    foreignKeys = [
        ForeignKey(
            entity = DeviceEntity::class,
            parentColumns = ["deviceId"],
            childColumns = ["deviceId"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
)
data class RideEntity(
    @androidx.room.PrimaryKey val rideId: String,
    val deviceId: Long,
    val formatVersion: Int,
    val startedAtUtcMs: Long?,
    val finishedAtUtcMs: Long?,
    val finished: Boolean,
    val distanceM: Double,
    val movingTimeMs: Long,
    val elapsedTimeMs: Long,
    val averageSpeedKmh: Double,
    val maxSpeedKmh: Double,
    val syncRevision: Long,
    val synced: Boolean,
)

@Entity(
    tableName = "ride_samples",
    primaryKeys = ["rideId", "sampleIndex"],
    indices = [Index("rideId")],
    foreignKeys = [
        ForeignKey(
            entity = RideEntity::class,
            parentColumns = ["rideId"],
            childColumns = ["rideId"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
)
data class RideSampleEntity(
    val rideId: String,
    val sampleIndex: Long,
    val elapsedMs: Long,
    val speedKmh: Double,
    val distanceM: Double,
    val pulseCount: Long,
)

@Entity(
    tableName = "ride_events",
    primaryKeys = ["rideId", "eventIndex"],
    indices = [Index("rideId")],
    foreignKeys = [
        ForeignKey(
            entity = RideEntity::class,
            parentColumns = ["rideId"],
            childColumns = ["rideId"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
)
data class RideEventEntity(
    val rideId: String,
    val eventIndex: Long,
    val elapsedMs: Long,
    val type: String,
    val detail: String?,
)

@Entity(
    tableName = "gps_points",
    primaryKeys = ["rideId", "pointIndex"],
    indices = [Index("rideId")],
    foreignKeys = [
        ForeignKey(
            entity = RideEntity::class,
            parentColumns = ["rideId"],
            childColumns = ["rideId"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
)
data class GpsPointEntity(
    val rideId: String,
    val pointIndex: Long,
    val timestampUtcMs: Long,
    val latitude: Double,
    val longitude: Double,
    val altitudeM: Double?,
    val accuracyM: Float?,
    val diagnosticGpsSpeedMps: Float?,
)

@Entity(
    tableName = "ride_files",
    primaryKeys = ["rideId", "fileId"],
    indices = [Index("rideId")],
    foreignKeys = [
        ForeignKey(
            entity = RideEntity::class,
            parentColumns = ["rideId"],
            childColumns = ["rideId"],
            onDelete = ForeignKey.CASCADE,
        ),
    ],
)
data class RideFileEntity(
    val rideId: String,
    val fileId: Int,
    val expectedSize: Long,
    val expectedCrc32: Long,
    val downloadedBytes: Long,
    val partialPath: String,
    val verifiedPath: String?,
    val verified: Boolean,
)
