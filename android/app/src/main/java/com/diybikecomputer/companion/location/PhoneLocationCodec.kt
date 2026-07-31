package com.diybikecomputer.companion.location

import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.roundToInt
import kotlin.math.roundToLong

data class PhoneLocationFix(
    val rideId: Long,
    val timestampUtcMs: Long,
    val latitude: Double,
    val longitude: Double,
    val altitudeM: Double?,
    val accuracyM: Float?,
    val speedMps: Float?,
)

object PhoneLocationCodec {
    const val PAYLOAD_BYTES = 33
    const val HAS_ALTITUDE = 1
    const val HAS_ACCURACY = 1 shl 1
    const val HAS_SPEED = 1 shl 2

    fun encode(fix: PhoneLocationFix): ByteArray? {
        if (fix.rideId !in 1..0xFFFF_FFFFL ||
            fix.timestampUtcMs !in MIN_TIMESTAMP_UTC_MS..MAX_TIMESTAMP_UTC_MS ||
            !fix.latitude.isFinite() || fix.latitude !in -90.0..90.0 ||
            !fix.longitude.isFinite() || fix.longitude !in -180.0..180.0
        ) {
            return null
        }
        val altitude = fix.altitudeM?.takeIf { it.isFinite() && it in -2_000.0..20_000.0 }
        val accuracy = fix.accuracyM?.takeIf { it.isFinite() && it > 0f && it <= 1_000f }
        val speed = fix.speedMps?.takeIf { it.isFinite() && it >= 0f && it <= 200f }
        var flags = 0
        if (altitude != null) flags = flags or HAS_ALTITUDE
        if (accuracy != null) flags = flags or HAS_ACCURACY
        if (speed != null) flags = flags or HAS_SPEED

        return ByteBuffer.allocate(PAYLOAD_BYTES)
            .order(ByteOrder.LITTLE_ENDIAN)
            .putInt(fix.rideId.toInt())
            .putLong(fix.timestampUtcMs)
            .put(flags.toByte())
            .putInt((fix.latitude * 10_000_000.0).roundToInt())
            .putInt((fix.longitude * 10_000_000.0).roundToInt())
            .putInt(altitude?.times(1_000.0)?.roundToInt() ?: 0)
            .putInt(accuracy?.times(1_000f)?.roundToLong()?.toInt() ?: 0)
            .putInt(speed?.times(1_000f)?.roundToLong()?.toInt() ?: 0)
            .array()
    }

    private const val MIN_TIMESTAMP_UTC_MS = 1_577_836_800_000L
    private const val MAX_TIMESTAMP_UTC_MS = 4_102_444_800_000L
}
