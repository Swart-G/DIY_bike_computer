package com.diybikecomputer.companion.rides

import androidx.room.Dao
import androidx.room.Database
import androidx.room.Query
import androidx.room.RoomDatabase
import androidx.room.Upsert
import androidx.room.migration.Migration
import androidx.sqlite.db.SupportSQLiteDatabase
import kotlinx.coroutines.flow.Flow

@Dao
interface DeviceDao {
    @Upsert
    suspend fun upsert(device: DeviceEntity)

    @Query("SELECT * FROM devices ORDER BY lastSeenUtcMs DESC")
    fun observeAll(): Flow<List<DeviceEntity>>
}

@Dao
interface RideDao {
    @Upsert
    suspend fun upsertRide(ride: RideEntity)

    @Upsert
    suspend fun upsertSamples(samples: List<RideSampleEntity>)

    @Upsert
    suspend fun upsertEvents(events: List<RideEventEntity>)

    @Upsert
    suspend fun upsertGpsPoints(points: List<GpsPointEntity>)

    @Upsert
    suspend fun upsertFile(file: RideFileEntity)

    @Query("SELECT * FROM ride_files WHERE rideId = :rideId AND fileId = :fileId")
    suspend fun getFile(rideId: String, fileId: Int): RideFileEntity?

    @Query("SELECT * FROM ride_files WHERE rideId = :rideId ORDER BY fileId")
    suspend fun getFiles(rideId: String): List<RideFileEntity>

    @Query("UPDATE rides SET synced = :synced WHERE rideId = :rideId")
    suspend fun setSynced(rideId: String, synced: Boolean)

    @Query("SELECT * FROM rides ORDER BY COALESCE(startedAtUtcMs, 0) DESC")
    fun observeRides(): Flow<List<RideEntity>>

    @Query("SELECT * FROM rides WHERE rideId = :rideId")
    fun observeRide(rideId: String): Flow<RideEntity?>

    @Query("SELECT * FROM rides WHERE rideId = :rideId")
    suspend fun getRide(rideId: String): RideEntity?

    @Query("SELECT * FROM ride_samples WHERE rideId = :rideId ORDER BY sampleIndex")
    fun observeSamples(rideId: String): Flow<List<RideSampleEntity>>

    @Query("SELECT * FROM ride_events WHERE rideId = :rideId ORDER BY eventIndex")
    fun observeEvents(rideId: String): Flow<List<RideEventEntity>>

    @Query("SELECT * FROM gps_points WHERE rideId = :rideId ORDER BY pointIndex")
    fun observeGpsPoints(rideId: String): Flow<List<GpsPointEntity>>

    @Query("SELECT * FROM gps_points WHERE rideId = :rideId ORDER BY pointIndex")
    suspend fun getGpsPoints(rideId: String): List<GpsPointEntity>

    @Query("SELECT COALESCE(MAX(pointIndex), -1) FROM gps_points WHERE rideId = :rideId")
    suspend fun lastGpsPointIndex(rideId: String): Long

    @Query("DELETE FROM ride_samples WHERE rideId = :rideId")
    suspend fun deleteSamples(rideId: String)

    @Query("DELETE FROM ride_events WHERE rideId = :rideId")
    suspend fun deleteEvents(rideId: String)
}

@Database(
    entities = [
        DeviceEntity::class,
        RideEntity::class,
        RideSampleEntity::class,
        RideEventEntity::class,
        GpsPointEntity::class,
        RideFileEntity::class,
    ],
    version = 2,
    exportSchema = true,
)
abstract class RideDatabase : RoomDatabase() {
    abstract fun deviceDao(): DeviceDao
    abstract fun rideDao(): RideDao

    companion object {
        val MIGRATION_1_2 = object : Migration(1, 2) {
            override fun migrate(db: SupportSQLiteDatabase) {
                db.execSQL(
                    """
                    CREATE TABLE IF NOT EXISTS `ride_files` (
                        `rideId` TEXT NOT NULL,
                        `fileId` INTEGER NOT NULL,
                        `expectedSize` INTEGER NOT NULL,
                        `expectedCrc32` INTEGER NOT NULL,
                        `downloadedBytes` INTEGER NOT NULL,
                        `partialPath` TEXT NOT NULL,
                        `verifiedPath` TEXT,
                        `verified` INTEGER NOT NULL,
                        PRIMARY KEY(`rideId`, `fileId`),
                        FOREIGN KEY(`rideId`) REFERENCES `rides`(`rideId`)
                            ON UPDATE NO ACTION ON DELETE CASCADE
                    )
                    """.trimIndent(),
                )
                db.execSQL(
                    "CREATE INDEX IF NOT EXISTS `index_ride_files_rideId` ON `ride_files` (`rideId`)",
                )
            }
        }
    }
}
