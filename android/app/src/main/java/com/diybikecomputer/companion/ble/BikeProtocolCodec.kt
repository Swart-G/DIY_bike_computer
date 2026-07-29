package com.diybikecomputer.companion.ble

import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.nio.ByteOrder

object BikeProtocol {
    const val VERSION: Int = 1
    const val HEADER_BYTES: Int = 9
    const val OVERHEAD_BYTES: Int = 11
    const val MAX_PAYLOAD_BYTES: Int = 503
    const val MAX_FRAME_BYTES: Int = 514

    const val SERVICE_UUID = "f5c10000-7d2a-4b8e-9c31-5a7e2d9b0001"
    const val DEVICE_INFO_UUID = "f5c10001-7d2a-4b8e-9c31-5a7e2d9b0001"
    const val RX_UUID = "f5c10002-7d2a-4b8e-9c31-5a7e2d9b0001"
    const val TX_UUID = "f5c10003-7d2a-4b8e-9c31-5a7e2d9b0001"

    object Message {
        const val HELLO = 0x01
        const val HELLO_ACK = 0x02
        const val ERROR = 0x03
        const val PING = 0x04
        const val PONG = 0x05
        const val TIME_SYNC = 0x10
        const val LIVE_TELEMETRY = 0x20
        const val RIDE_EVENT = 0x21
        const val DEVICE_EVENT = 0x22
        const val RIDE_LIST_REQUEST = 0x30
        const val RIDE_MANIFEST = 0x31
        const val RIDE_LIST_END = 0x32
        const val RIDE_DOWNLOAD_REQUEST = 0x33
        const val FILE_BEGIN = 0x34
        const val FILE_CHUNK = 0x35
        const val FILE_ACK = 0x36
        const val FILE_END = 0x37
        const val TRANSFER_CANCEL = 0x38
        const val MEDIA_STATE = 0x40
        const val MEDIA_ACTION = 0x41
        const val NAVIGATION_STATE = 0x50
        const val CONFIG_GET = 0x60
        const val CONFIG_VALUE = 0x61
        const val CONFIG_SET = 0x62
        const val CONFIG_RESULT = 0x63
    }

    object Flag {
        const val ACK_REQUIRED = 0x01
        const val RESPONSE = 0x02
        const val ERROR = 0x04
        const val MORE = 0x08
        const val PRIVILEGED = 0x10
    }

    const val CAPABILITIES =
        (1 shl 0) or (1 shl 1) or (1 shl 2) or
            (1 shl 3) or (1 shl 4) or (1 shl 5)
}

data class BikeFrame(
    val messageType: Int,
    val flags: Int,
    val sequence: Int,
    val payload: ByteArray,
)

sealed interface DecodeEvent {
    data class Frame(val value: BikeFrame) : DecodeEvent
    data object NeedMoreData : DecodeEvent
    data object InvalidMagic : DecodeEvent
    data object UnsupportedVersion : DecodeEvent
    data object PayloadTooLarge : DecodeEvent
    data object CrcMismatch : DecodeEvent
    data object BufferOverflow : DecodeEvent
}

object BikeProtocolCodec {
    fun crc16(data: ByteArray, length: Int = data.size): Int {
        var crc = 0xFFFF
        repeat(length) { index ->
            crc = crc xor ((data[index].toInt() and 0xFF) shl 8)
            repeat(8) {
                crc = if ((crc and 0x8000) != 0) {
                    ((crc shl 1) xor 0x1021) and 0xFFFF
                } else {
                    (crc shl 1) and 0xFFFF
                }
            }
        }
        return crc
    }

    fun encode(
        messageType: Int,
        flags: Int,
        sequence: Int,
        payload: ByteArray = byteArrayOf(),
    ): ByteArray {
        require(payload.size <= BikeProtocol.MAX_PAYLOAD_BYTES)
        val result = ByteArray(payload.size + BikeProtocol.OVERHEAD_BYTES)
        val buffer = ByteBuffer.wrap(result).order(ByteOrder.LITTLE_ENDIAN)
        buffer.put('B'.code.toByte())
        buffer.put('C'.code.toByte())
        buffer.put(BikeProtocol.VERSION.toByte())
        buffer.put(messageType.toByte())
        buffer.put(flags.toByte())
        buffer.putShort(sequence.toShort())
        buffer.putShort(payload.size.toShort())
        buffer.put(payload)
        buffer.putShort(crc16(result, BikeProtocol.HEADER_BYTES + payload.size).toShort())
        return result
    }
}

class BikeProtocolDecoder {
    private val bytes = ByteArrayOutputStream(BikeProtocol.MAX_FRAME_BYTES * 2)
    private var overflowPending = false

    fun feed(input: ByteArray) {
        if (bytes.size() + input.size > BikeProtocol.MAX_FRAME_BYTES * 2) {
            reset()
            overflowPending = true
            return
        }
        bytes.write(input)
    }

    fun next(): DecodeEvent {
        if (overflowPending) {
            overflowPending = false
            return DecodeEvent.BufferOverflow
        }
        val input = bytes.toByteArray()
        if (input.size < 2) return DecodeEvent.NeedMoreData
        var magicOffset = 0
        while (magicOffset + 1 < input.size &&
            (input[magicOffset] != 'B'.code.toByte() ||
                input[magicOffset + 1] != 'C'.code.toByte())
        ) {
            magicOffset++
        }
        if (magicOffset > 0) {
            discard(input, magicOffset)
            return DecodeEvent.InvalidMagic
        }
        if (input.size < BikeProtocol.HEADER_BYTES) return DecodeEvent.NeedMoreData
        if ((input[2].toInt() and 0xFF) != BikeProtocol.VERSION) {
            discard(input, 2)
            return DecodeEvent.UnsupportedVersion
        }
        val payloadLength =
            (input[7].toInt() and 0xFF) or ((input[8].toInt() and 0xFF) shl 8)
        if (payloadLength > BikeProtocol.MAX_PAYLOAD_BYTES) {
            discard(input, 2)
            return DecodeEvent.PayloadTooLarge
        }
        val frameLength = payloadLength + BikeProtocol.OVERHEAD_BYTES
        if (input.size < frameLength) return DecodeEvent.NeedMoreData
        val expected =
            (input[9 + payloadLength].toInt() and 0xFF) or
                ((input[10 + payloadLength].toInt() and 0xFF) shl 8)
        val actual = BikeProtocolCodec.crc16(input, BikeProtocol.HEADER_BYTES + payloadLength)
        if (expected != actual) {
            discard(input, 2)
            return DecodeEvent.CrcMismatch
        }
        val frame = BikeFrame(
            messageType = input[3].toInt() and 0xFF,
            flags = input[4].toInt() and 0xFF,
            sequence =
                (input[5].toInt() and 0xFF) or ((input[6].toInt() and 0xFF) shl 8),
            payload = input.copyOfRange(9, 9 + payloadLength),
        )
        discard(input, frameLength)
        return DecodeEvent.Frame(frame)
    }

    fun reset() {
        bytes.reset()
        overflowPending = false
    }

    private fun discard(input: ByteArray, count: Int) {
        bytes.reset()
        if (count < input.size) bytes.write(input, count, input.size - count)
    }
}

fun String.hexBytes(): ByteArray {
    require(length % 2 == 0)
    return chunked(2).map { it.toInt(16).toByte() }.toByteArray()
}

fun ByteArray.hex(): String = joinToString("") { "%02x".format(it.toInt() and 0xFF) }
