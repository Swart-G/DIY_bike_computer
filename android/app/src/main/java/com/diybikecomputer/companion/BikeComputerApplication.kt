package com.diybikecomputer.companion

import android.app.Application
import androidx.room.Room
import com.diybikecomputer.companion.ble.BikeConnectionService
import com.diybikecomputer.companion.rides.RideDatabase
import com.diybikecomputer.companion.rides.RideSyncManager
import com.diybikecomputer.companion.location.GpsRepository
import com.diybikecomputer.companion.location.RideGpsCoordinator
import com.diybikecomputer.companion.media.MediaBleCoordinator
import com.diybikecomputer.companion.media.MediaRepository
import com.diybikecomputer.companion.navigation.NavigationBleCoordinator
import com.diybikecomputer.companion.navigation.NavigationRepository
import com.diybikecomputer.companion.settings.DeviceSettingsRepository

class BikeComputerApplication : Application() {
    val connection: BikeConnectionService by lazy { BikeConnectionService(this) }
    val database: RideDatabase by lazy {
        Room.databaseBuilder(this, RideDatabase::class.java, "bike-computer.db")
            .addMigrations(RideDatabase.MIGRATION_1_2)
            .build()
    }
    val rideSync: RideSyncManager by lazy {
        RideSyncManager(this, connection, database)
    }
    val gpsRepository: GpsRepository by lazy { GpsRepository(this) }
    val gpsCoordinator: RideGpsCoordinator by lazy {
        RideGpsCoordinator(this, connection, gpsRepository)
    }
    val mediaRepository: MediaRepository by lazy { MediaRepository(this) }
    val mediaCoordinator: MediaBleCoordinator by lazy {
        MediaBleCoordinator(connection, mediaRepository)
    }
    val navigationRepository: NavigationRepository by lazy { NavigationRepository() }
    val navigationCoordinator: NavigationBleCoordinator by lazy {
        NavigationBleCoordinator(connection, navigationRepository)
    }
    val deviceSettings: DeviceSettingsRepository by lazy {
        DeviceSettingsRepository(connection)
    }

    override fun onCreate() {
        super.onCreate()
        rideSync
        gpsCoordinator
        mediaCoordinator
        navigationCoordinator
        deviceSettings
    }
}
