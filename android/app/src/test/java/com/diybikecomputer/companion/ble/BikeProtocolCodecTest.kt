package com.diybikecomputer.companion.ble

import java.io.File
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class BikeProtocolCodecTest {
    private fun vectors(): JSONObject {
        val file = File("../../docs/protocol_test_vectors.json")
        assertTrue("Shared protocol vectors must exist at ${file.absolutePath}", file.isFile)
        return JSONObject(file.readText())
    }

    @Test
    fun sharedValidVectorsDecode() {
        val entries = vectors().getJSONArray("vectors")
        for (index in 0 until entries.length()) {
            val vector = entries.getJSONObject(index)
            if (vector.optString("expected") != "frame" ||
                !vector.has("input_hex") ||
                vector.has("feed_after")
            ) {
                continue
            }
            val decoder = BikeProtocolDecoder()
            decoder.feed(vector.getString("input_hex").hexBytes())
            val event = decoder.next()
            assertTrue(vector.getString("name"), event is DecodeEvent.Frame)
            val frame = (event as DecodeEvent.Frame).value
            assertEquals(vector.getInt("message_type"), frame.messageType)
            assertEquals(vector.getInt("sequence"), frame.sequence)
            assertEquals(vector.optString("payload_hex"), frame.payload.hex())
        }
    }

    @Test
    fun sharedPingVectorEncodesExactly() {
        val expected = vectors().getJSONArray("vectors").getJSONObject(1).getString("input_hex")
        val encoded = BikeProtocolCodec.encode(
            BikeProtocol.Message.PING,
            0,
            42,
            "78563412".hexBytes(),
        )
        assertEquals(expected, encoded.hex())
    }

    @Test
    fun partialAndMultipleFramesAreStreamSafe() {
        val entries = vectors().getJSONArray("vectors")
        val decoder = BikeProtocolDecoder()
        decoder.feed(entries.getJSONObject(5).getString("input_hex").hexBytes())
        assertEquals(DecodeEvent.NeedMoreData, decoder.next())
        decoder.feed(entries.getJSONObject(6).getString("input_hex").hexBytes())
        assertTrue(decoder.next() is DecodeEvent.Frame)

        decoder.feed(entries.getJSONObject(7).getString("input_hex").hexBytes())
        assertTrue(decoder.next() is DecodeEvent.Frame)
        assertTrue(decoder.next() is DecodeEvent.Frame)
        assertEquals(DecodeEvent.NeedMoreData, decoder.next())
    }

    @Test
    fun maximumPayloadMatchesSharedCrc() {
        val payload = ByteArray(BikeProtocol.MAX_PAYLOAD_BYTES) { it.toByte() }
        val frame = BikeProtocolCodec.encode(BikeProtocol.Message.PING, 0, 0xFFFF, payload)
        assertEquals(BikeProtocol.MAX_FRAME_BYTES, frame.size)
        assertEquals("ced5", frame.copyOfRange(frame.size - 2, frame.size).hex())
    }
}
