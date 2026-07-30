package com.diybikecomputer.companion.device

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject

data class KnownDevice(
    val deviceId: Long,
    val associationId: Long,
    val displayName: String,
    val bluetoothAddress: String,
    val systemAssociationId: Int? = null,
    val lastSeenUtcMs: Long = 0,
) {
    val authorized: Boolean
        get() = deviceId != 0L && associationId != 0L
}

class DeviceRepository(context: Context) {
    private val preferences =
        context.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE)
    private val mutableDevices = MutableStateFlow(loadAndMigrate())
    val devices: StateFlow<List<KnownDevice>> = mutableDevices.asStateFlow()

    @Synchronized
    fun knownDevices(): List<KnownDevice> = mutableDevices.value

    @Synchronized
    fun knownDevice(): KnownDevice? {
        val activeAddress = preferences.getString(ACTIVE_ADDRESS, null)
        return mutableDevices.value.firstOrNull {
            it.bluetoothAddress.equals(activeAddress, ignoreCase = true)
        } ?: mutableDevices.value.maxByOrNull { it.lastSeenUtcMs }
    }

    @Synchronized
    fun find(bluetoothAddress: String): KnownDevice? =
        mutableDevices.value.firstOrNull {
            it.bluetoothAddress.equals(bluetoothAddress, ignoreCase = true)
        }

    @Synchronized
    fun select(bluetoothAddress: String) {
        preferences.edit().putString(ACTIVE_ADDRESS, bluetoothAddress).apply()
    }

    @Synchronized
    fun rememberEndpoint(
        bluetoothAddress: String,
        displayName: String?,
        systemAssociationId: Int? = null,
    ) {
        val previous = find(bluetoothAddress)
        upsert(
            previous?.copy(
                displayName = displayName?.takeIf(String::isNotBlank)
                    ?: previous.displayName,
                systemAssociationId = systemAssociationId ?: previous.systemAssociationId,
                lastSeenUtcMs = System.currentTimeMillis(),
            ) ?: KnownDevice(
                deviceId = 0,
                associationId = 0,
                displayName = displayName?.takeIf(String::isNotBlank) ?: DEFAULT_NAME,
                bluetoothAddress = bluetoothAddress,
                systemAssociationId = systemAssociationId,
                lastSeenUtcMs = System.currentTimeMillis(),
            ),
        )
        select(bluetoothAddress)
    }

    @Synchronized
    fun save(device: KnownDevice) {
        upsert(device.copy(lastSeenUtcMs = System.currentTimeMillis()))
        select(device.bluetoothAddress)
    }

    @Synchronized
    fun forget(bluetoothAddress: String): KnownDevice? {
        val removed = find(bluetoothAddress) ?: return null
        val remaining = mutableDevices.value.filterNot {
            it.bluetoothAddress.equals(bluetoothAddress, ignoreCase = true)
        }
        persist(remaining)
        if (preferences.getString(ACTIVE_ADDRESS, null)
                .equals(bluetoothAddress, ignoreCase = true)
        ) {
            val next = remaining.maxByOrNull { it.lastSeenUtcMs }
            preferences.edit().apply {
                if (next == null) remove(ACTIVE_ADDRESS)
                else putString(ACTIVE_ADDRESS, next.bluetoothAddress)
            }.apply()
        }
        return removed
    }

    @Synchronized
    private fun upsert(device: KnownDevice) {
        val next = mutableDevices.value.toMutableList()
        val index = next.indexOfFirst {
            it.bluetoothAddress.equals(device.bluetoothAddress, ignoreCase = true)
        }
        if (index >= 0) next[index] = device else next += device
        persist(next.sortedByDescending { it.lastSeenUtcMs })
    }

    @Synchronized
    private fun persist(devices: List<KnownDevice>) {
        val json = JSONArray()
        devices.forEach { device ->
            json.put(
                JSONObject()
                    .put("device_id", device.deviceId)
                    .put("association_id", device.associationId)
                    .put("display_name", device.displayName)
                    .put("bluetooth_address", device.bluetoothAddress)
                    .put("system_association_id", device.systemAssociationId ?: -1)
                    .put("last_seen_utc_ms", device.lastSeenUtcMs),
            )
        }
        preferences.edit().putString(DEVICES_JSON, json.toString()).apply()
        mutableDevices.value = devices
    }

    private fun loadAndMigrate(): List<KnownDevice> {
        val stored = preferences.getString(DEVICES_JSON, null)
        if (!stored.isNullOrBlank()) {
            return runCatching {
                val json = JSONArray(stored)
                buildList {
                    repeat(json.length()) { index ->
                        val item = json.getJSONObject(index)
                        val address = item.optString("bluetooth_address")
                        if (address.isNotBlank()) {
                            add(
                                KnownDevice(
                                    deviceId = item.optLong("device_id"),
                                    associationId = item.optLong("association_id"),
                                    displayName = item.optString("display_name", DEFAULT_NAME),
                                    bluetoothAddress = address,
                                    systemAssociationId =
                                        item.optInt("system_association_id", -1)
                                            .takeIf { it >= 0 },
                                    lastSeenUtcMs = item.optLong("last_seen_utc_ms"),
                                ),
                            )
                        }
                    }
                }
            }.getOrDefault(emptyList())
        }

        val legacyAddress = preferences.getString("bluetooth_address", null)
            ?: return emptyList()
        val migrated = KnownDevice(
            deviceId = preferences.getLong("device_id", 0),
            associationId = preferences.getLong("association_id", 0),
            displayName = preferences.getString("display_name", DEFAULT_NAME)
                .orEmpty().ifBlank { DEFAULT_NAME },
            bluetoothAddress = legacyAddress,
            lastSeenUtcMs = System.currentTimeMillis(),
        )
        val json = JSONArray().put(
            JSONObject()
                .put("device_id", migrated.deviceId)
                .put("association_id", migrated.associationId)
                .put("display_name", migrated.displayName)
                .put("bluetooth_address", migrated.bluetoothAddress)
                .put("system_association_id", -1)
                .put("last_seen_utc_ms", migrated.lastSeenUtcMs),
        )
        preferences.edit()
            .putString(DEVICES_JSON, json.toString())
            .putString(ACTIVE_ADDRESS, legacyAddress)
            .remove("device_id")
            .remove("association_id")
            .remove("display_name")
            .remove("bluetooth_address")
            .apply()
        return listOf(migrated)
    }

    companion object {
        private const val PREFERENCES = "bike_device_association"
        private const val DEVICES_JSON = "known_devices_json"
        private const val ACTIVE_ADDRESS = "active_bluetooth_address"
        private const val DEFAULT_NAME = "DIY Bike Computer"
    }
}
