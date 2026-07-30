package com.diybikecomputer.companion.device

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanFilter
import android.companion.AssociationInfo
import android.companion.AssociationRequest
import android.companion.BluetoothLeDeviceFilter
import android.companion.CompanionDeviceManager
import android.content.Context
import android.content.IntentSender
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import com.diybikecomputer.companion.ble.BikeProtocol
import java.util.UUID

class CompanionDevicePairing(context: Context) {
    interface Callback {
        fun onChooser(intentSender: IntentSender)
        fun onAssociated(association: AssociationInfo)
        fun onFailure(reason: String)
    }

    private val manager =
        context.getSystemService(CompanionDeviceManager::class.java)
    private val bluetoothAdapter =
        context.getSystemService(BluetoothManager::class.java)?.adapter

    @SuppressLint("MissingPermission")
    fun deviceFor(association: AssociationInfo): BluetoothDevice? {
        if (Build.VERSION.SDK_INT >= 34) {
            val associatedDevice = association.associatedDevice
            val directDevice = associatedDevice?.bleDevice?.device
                ?: associatedDevice?.bluetoothDevice
            if (directDevice != null) return directDevice
        }
        return if (Build.VERSION.SDK_INT >= 33) {
            remoteDevice(association.deviceMacAddress?.toString())
        } else {
            null
        }
    }

    @SuppressLint("MissingPermission")
    @Suppress("DEPRECATION")
    fun latestAssociatedDevice(): BluetoothDevice? {
        return runCatching {
            if (Build.VERSION.SDK_INT >= 33) {
                manager.myAssociations
                    .maxByOrNull(AssociationInfo::getId)
                    ?.let(::deviceFor)
            } else {
                remoteDevice(manager.associations.lastOrNull())
            }
        }.getOrNull()
    }

    fun systemAssociationIdFor(address: String): Int? {
        if (Build.VERSION.SDK_INT < 33) return null
        return runCatching {
            manager.myAssociations.firstOrNull {
                it.deviceMacAddress?.toString().equals(address, ignoreCase = true)
            }?.id
        }.getOrNull()
    }

    @Suppress("DEPRECATION")
    fun disassociate(systemAssociationId: Int?, bluetoothAddress: String): Boolean =
        runCatching {
            if (Build.VERSION.SDK_INT >= 33 && systemAssociationId != null) {
                manager.disassociate(systemAssociationId)
            } else {
                manager.disassociate(bluetoothAddress)
            }
        }.isSuccess

    fun associate(callback: Callback) {
        val scanFilter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(UUID.fromString(BikeProtocol.SERVICE_UUID)))
            .build()
        val deviceFilter = BluetoothLeDeviceFilter.Builder()
            .setScanFilter(scanFilter)
            .build()
        val request = AssociationRequest.Builder()
            .addDeviceFilter(deviceFilter)
            .setSingleDevice(false)
            .build()
        try {
            manager.associate(
                request,
                object : CompanionDeviceManager.Callback() {
                    @Deprecated("Required for Android 12 and earlier")
                    override fun onDeviceFound(chooserLauncher: IntentSender) {
                        callback.onChooser(chooserLauncher)
                    }

                    override fun onAssociationPending(intentSender: IntentSender) {
                        callback.onChooser(intentSender)
                    }

                    override fun onAssociationCreated(associationInfo: AssociationInfo) {
                        callback.onAssociated(associationInfo)
                    }

                    override fun onFailure(error: CharSequence?) {
                        callback.onFailure(error?.toString() ?: "Association failed")
                    }
                },
                Handler(Looper.getMainLooper()),
            )
        } catch (error: RuntimeException) {
            callback.onFailure(error.message ?: "Unable to start device association")
        }
    }

    @SuppressLint("MissingPermission")
    private fun remoteDevice(address: String?): BluetoothDevice? {
        if (address == null) return null
        return runCatching { bluetoothAdapter?.getRemoteDevice(address) }.getOrNull()
    }
}
