package com.diybikecomputer.companion.rides

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.util.zip.ZipInputStream
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RideSummaryXlsxTest {
    @Test
    fun writesMinimalValidWorkbookWithSummaryOnly() {
        val output = ByteArrayOutputStream()
        RideSummaryXlsx.write(
            output,
            RideEntity(
                rideId = "42:7",
                deviceId = 42,
                formatVersion = 1,
                startedAtUtcMs = 1_700_000_000_000,
                finishedAtUtcMs = 1_700_003_600_000,
                finished = true,
                distanceM = 12_340.0,
                movingTimeMs = 3_000_000,
                elapsedTimeMs = 3_600_000,
                averageSpeedKmh = 14.8,
                maxSpeedKmh = 38.2,
                syncRevision = 2,
                synced = true,
            ),
            gpsPointCount = 123,
        )

        val entries = linkedMapOf<String, String>()
        ZipInputStream(ByteArrayInputStream(output.toByteArray())).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                entries[entry.name] = zip.readBytes().decodeToString()
            }
        }

        assertEquals(
            setOf(
                "[Content_Types].xml",
                "_rels/.rels",
                "xl/workbook.xml",
                "xl/_rels/workbook.xml.rels",
                "xl/styles.xml",
                "xl/worksheets/sheet1.xml",
                "docProps/core.xml",
                "docProps/app.xml",
            ),
            entries.keys,
        )
        val sheet = entries.getValue("xl/worksheets/sheet1.xml")
        assertTrue(sheet.contains("Ride summary"))
        assertTrue(sheet.contains("12.34"))
        assertTrue(sheet.contains("38.2"))
        assertTrue(sheet.contains("GPS points"))
        assertTrue(sheet.contains(">123<"))
        assertTrue(!sheet.contains("sample_index"))
    }
}
