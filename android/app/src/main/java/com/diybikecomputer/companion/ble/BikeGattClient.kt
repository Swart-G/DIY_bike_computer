package com.diybikecomputer.companion.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.util.UUID
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow

enum class GattState {
    Disconnected,
    Connecting,
    Discovering,
    Subscribing,
    Ready,
    Error,
}

@SuppressLint("MissingPermission")
@Suppress("DEPRECATION")
class BikeGattClient(private val context: Context) {
    private val serviceUuid = UUID.fromString(BikeProtocol.SERVICE_UUID)
    private val rxUuid = UUID.fromString(BikeProtocol.RX_UUID)
    private val txUuid = UUID.fromString(BikeProtocol.TX_UUID)
    private val cccdUuid = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    private val mutableState = MutableStateFlow(GattState.Disconnected)
    val state: StateFlow<GattState> = mutableState.asStateFlow()

    private val mutableIncoming = MutableSharedFlow<ByteArray>(extraBufferCapacity = 16)
    val incoming: SharedFlow<ByteArray> = mutableIncoming.asSharedFlow()

    private val callbackHandler = Handler(Looper.getMainLooper())
    private val pendingWrites = ArrayDeque<ByteArray>()
    private var writeInFlight = false
    private var writeGeneration = 0L
    @Volatile
    private var gatt: BluetoothGatt? = null
    @Volatile
    private var rx: BluetoothGattCharacteristic? = null
    @Volatile
    private var tx: BluetoothGattCharacteristic? = null
    @Volatile
    var negotiatedMtu: Int = 23
        private set

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (this@BikeGattClient.gatt !== gatt) {
                runCatching { gatt.close() }
                return
            }
            if (status == BluetoothGatt.GATT_SUCCESS &&
                newState == BluetoothProfile.STATE_CONNECTED
            ) {
                mutableState.value = GattState.Discovering
                val mtuRequested = runCatching { gatt.requestMtu(247) }.getOrDefault(false)
                if (!mtuRequested &&
                    !runCatching { gatt.discoverServices() }.getOrDefault(false)
                ) {
                    mutableState.value = GattState.Error
                }
            } else {
                clearConnection(gatt, status != BluetoothGatt.GATT_SUCCESS)
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (this@BikeGattClient.gatt !== gatt) return
            if (status == BluetoothGatt.GATT_SUCCESS) negotiatedMtu = mtu
            if (!runCatching { gatt.discoverServices() }.getOrDefault(false)) {
                mutableState.value = GattState.Error
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (this@BikeGattClient.gatt !== gatt) return
            val service: BluetoothGattService? =
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    runCatching { gatt.getService(serviceUuid) }.getOrNull()
                } else {
                    null
                }
            rx = service?.getCharacteristic(rxUuid)
            tx = service?.getCharacteristic(txUuid)
            val txValue = tx
            if (rx == null || txValue == null ||
                !runCatching {
                    gatt.setCharacteristicNotification(txValue, true)
                }.getOrDefault(false)
            ) {
                mutableState.value = GattState.Error
                return
            }
            val descriptor = txValue.getDescriptor(cccdUuid)
            if (descriptor == null) {
                mutableState.value = GattState.Error
                return
            }
            mutableState.value = GattState.Subscribing
            writeDescriptor(gatt, descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int,
        ) {
            if (this@BikeGattClient.gatt !== gatt) return
            mutableState.value =
                if (status == BluetoothGatt.GATT_SUCCESS) GattState.Ready else GattState.Error
        }

        @Deprecated("Used on Android 12 and earlier")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
        ) {
            if (this@BikeGattClient.gatt !== gatt) return
            if (characteristic.uuid == txUuid) {
                publishIncoming(characteristic.value?.copyOf() ?: return)
            }
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            if (this@BikeGattClient.gatt !== gatt) return
            if (characteristic.uuid == txUuid) publishIncoming(value.copyOf())
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            if (this@BikeGattClient.gatt !== gatt) return
            synchronized(pendingWrites) {
                writeInFlight = false
                writeGeneration++
                if (status != BluetoothGatt.GATT_SUCCESS) {
                    pendingWrites.clear()
                    mutableState.value = GattState.Error
                    return
                }
            }
            pumpWrites()
        }
    }

    fun connect(device: BluetoothDevice) {
        closeActiveConnection()
        mutableState.value = GattState.Connecting
        val connection = runCatching {
            device.connectGatt(
                context,
                false,
                callback,
                BluetoothDevice.TRANSPORT_LE,
                BluetoothDevice.PHY_LE_1M_MASK,
                callbackHandler,
            )
        }.getOrNull()
        if (connection == null) {
            mutableState.value = GattState.Error
            return
        }
        gatt = connection
    }

    fun enqueueFrame(frame: ByteArray): Boolean {
        if (mutableState.value != GattState.Ready || frame.isEmpty()) return false
        val packetSize = (negotiatedMtu - 3).coerceAtLeast(20)
        val packets = ArrayList<ByteArray>((frame.size + packetSize - 1) / packetSize)
        var offset = 0
        while (offset < frame.size) {
            val end = (offset + packetSize).coerceAtMost(frame.size)
            packets += frame.copyOfRange(offset, end)
            offset = end
        }
        synchronized(pendingWrites) {
            if (pendingWrites.size + packets.size > 32) return false
            packets.forEach(pendingWrites::addLast)
        }
        pumpWrites()
        return true
    }

    fun close() {
        closeActiveConnection()
        mutableState.value = GattState.Disconnected
    }

    private fun closeActiveConnection() {
        val active = gatt
        gatt = null
        runCatching { active?.disconnect() }
        runCatching { active?.close() }
        synchronized(pendingWrites) {
            pendingWrites.clear()
            writeInFlight = false
            writeGeneration++
        }
        rx = null
        tx = null
        negotiatedMtu = 23
    }

    private fun pumpWrites() {
        val activeGatt = gatt ?: return
        val characteristic = rx ?: return
        var generation = 0L
        val value = synchronized(pendingWrites) {
            if (writeInFlight || pendingWrites.isEmpty()) return
            writeInFlight = true
            generation = ++writeGeneration
            pendingWrites.removeFirst()
        }
        val accepted = runCatching {
            if (Build.VERSION.SDK_INT >= 33) {
                activeGatt.writeCharacteristic(
                    characteristic,
                    value,
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
                ) == BluetoothStatusCodes.SUCCESS
            } else {
                characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                characteristic.value = value
                activeGatt.writeCharacteristic(characteristic)
            }
        }.getOrDefault(false)
        if (!accepted) {
            synchronized(pendingWrites) {
                writeInFlight = false
                writeGeneration++
                pendingWrites.addFirst(value)
            }
            mutableState.value = GattState.Error
            return
        }
        callbackHandler.postDelayed(
            {
                val timedOut = synchronized(pendingWrites) {
                    if (!writeInFlight || writeGeneration != generation) {
                        false
                    } else {
                        writeInFlight = false
                        writeGeneration++
                        pendingWrites.clear()
                        true
                    }
                }
                if (timedOut) mutableState.value = GattState.Error
            },
            WRITE_TIMEOUT_MS,
        )
    }

    private fun writeDescriptor(
        gatt: BluetoothGatt,
        descriptor: BluetoothGattDescriptor,
        value: ByteArray,
    ) {
        val accepted = runCatching {
            if (Build.VERSION.SDK_INT >= 33) {
                gatt.writeDescriptor(descriptor, value) == BluetoothStatusCodes.SUCCESS
            } else {
                descriptor.value = value
                gatt.writeDescriptor(descriptor)
            }
        }.getOrDefault(false)
        if (!accepted) {
            mutableState.value = GattState.Error
        }
    }

    private fun clearConnection(gatt: BluetoothGatt, error: Boolean) {
        if (this.gatt !== gatt) {
            runCatching { gatt.close() }
            return
        }
        this.gatt = null
        runCatching { gatt.close() }
        rx = null
        tx = null
        negotiatedMtu = 23
        synchronized(pendingWrites) {
            pendingWrites.clear()
            writeInFlight = false
            writeGeneration++
        }
        mutableState.value = if (error) GattState.Error else GattState.Disconnected
    }

    private fun publishIncoming(value: ByteArray) {
        if (!mutableIncoming.tryEmit(value)) {
            mutableState.value = GattState.Error
        }
    }

    private companion object {
        const val WRITE_TIMEOUT_MS = 5_000L
    }
}
