package com.diybikecomputer.companion.location

import java.nio.ByteBuffer
import java.nio.ByteOrder
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PhoneLocationCodecTest {
    @Test
    fun `encodes bounded location in firmware wire format`() {
        val payload = PhoneLocationCodec.encode(
            PhoneLocationFix(
                rideId = 42,
                timestampUtcMs = 1_800_000_000_123,
                latitude = 55.755826,
                longitude = 37.617300,
                altitudeM = 156.25,
                accuracyM = 3.5f,
                speedMps = 7.25f,
            ),
        )!!
        assertEquals(PhoneLocationCodec.PAYLOAD_BYTES, payload.size)
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)
        assertEquals(42, buffer.int)
        assertEquals(1_800_000_000_123, buffer.long)
        assertEquals(7, buffer.get().toInt())
        assertEquals(557_558_260, buffer.int)
        assertEquals(376_173_000, buffer.int)
        assertEquals(156_250, buffer.int)
        assertEquals(3_500, buffer.int)
        assertEquals(7_250, buffer.int)
    }

    @Test
    fun `invalid coordinates are never queued`() {
        val base = PhoneLocationFix(1, 1_800_000_000_000, 55.0, 37.0, null, null, null)
        assertNull(PhoneLocationCodec.encode(base.copy(latitude = 91.0)))
        assertNull(PhoneLocationCodec.encode(base.copy(longitude = Double.NaN)))
        assertNull(PhoneLocationCodec.encode(base.copy(rideId = 0)))
    }

    @Test
    fun `invalid optional diagnostics are omitted instead of fabricated`() {
        val payload = PhoneLocationCodec.encode(
            PhoneLocationFix(
                7,
                1_800_000_000_000,
                1.0,
                2.0,
                99_000.0,
                -1f,
                500f,
            ),
        )!!
        assertEquals(0, payload[12].toInt())
        assertTrue(payload.copyOfRange(21, payload.size).all { it == 0.toByte() })
    }
}
