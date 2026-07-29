package com.diybikecomputer.companion.device

import android.content.Context

data class KnownDevice(
    val deviceId: Long,
    val associationId: Long,
    val displayName: String,
)

class DeviceRepository(context: Context) {
    private val preferences =
        context.getSharedPreferences("bike_device_association", Context.MODE_PRIVATE)

    fun knownDevice(): KnownDevice? {
        if (!preferences.contains("association_id")) return null
        return KnownDevice(
            deviceId = preferences.getLong("device_id", 0),
            associationId = preferences.getLong("association_id", 0),
            displayName = preferences.getString("display_name", "DIY Bike Computer").orEmpty(),
        ).takeIf { it.associationId != 0L }
    }

    fun save(device: KnownDevice) {
        preferences.edit()
            .putLong("device_id", device.deviceId)
            .putLong("association_id", device.associationId)
            .putString("display_name", device.displayName)
            .apply()
    }

    fun knownBluetoothAddress(): String? =
        preferences.getString("bluetooth_address", null)

    fun saveBluetoothAddress(address: String) {
        preferences.edit().putString("bluetooth_address", address).apply()
    }

    fun forgetApplicationAssociation() {
        preferences.edit().clear().apply()
    }
}
