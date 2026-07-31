package com.diybikecomputer.companion.rides

import java.nio.file.Files
import org.junit.Assert.assertEquals
import org.junit.Test

class RideGpsFileReaderTest {
    @Test
    fun `reads unique validated device fixes from format two samples`() {
        val file = Files.createTempFile("ride-gps", ".csv").toFile()
        try {
            file.writeText(
                "sample_index,ride_time_ms,timestamp_utc_ms,latitude,longitude," +
                    "altitude_m,gps_accuracy_m,gps_speed_mps,gps_age_ms\n" +
                    "0,0,1800000000000,55.7558260,37.6173000,156.250,3.500,7.250,100\n" +
                    "1,1000,1800000000000,55.7558260,37.6173000,156.250,3.500,7.250,1100\n" +
                    "2,2000,,,,,,,\n" +
                    "3,3000,1800000003000,55.7559000,37.6174000,,4.000,,50\n" +
                    "4,4000,1800000004000,95.0,37.0,,4.0,,20\n",
            )
            val points = RideGpsFileReader.readFile(file)
            assertEquals(2, points.size)
            assertEquals(55.755826, points[0].latitude, 0.0000001)
            assertEquals(156.25, points[0].altitudeM!!, 0.001)
            assertEquals(1_800_000_003_000, points[1].timestampUtcMs)
            assertEquals(null, points[1].altitudeM)
        } finally {
            file.delete()
        }
    }
}
