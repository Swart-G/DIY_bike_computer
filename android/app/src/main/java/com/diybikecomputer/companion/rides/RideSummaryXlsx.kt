package com.diybikecomputer.companion.rides

import java.io.OutputStream
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

object RideSummaryXlsx {
    private val dateTimeFormatter =
        DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss").withZone(ZoneId.systemDefault())

    fun write(output: OutputStream, ride: RideEntity, gpsPointCount: Int) {
        ZipOutputStream(output).use { zip ->
            zip.writeEntry(
                "[Content_Types].xml",
                """
                <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
                <Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
                  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
                  <Default Extension="xml" ContentType="application/xml"/>
                  <Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>
                  <Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>
                  <Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>
                  <Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
                  <Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>
                </Types>
                """.trimIndent(),
            )
            zip.writeEntry(
                "_rels/.rels",
                """
                <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
                <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
                  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>
                  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>
                  <Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>
                </Relationships>
                """.trimIndent(),
            )
            zip.writeEntry(
                "xl/workbook.xml",
                """
                <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
                <workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"
                    xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
                  <sheets><sheet name="Ride summary" sheetId="1" r:id="rId1"/></sheets>
                </workbook>
                """.trimIndent(),
            )
            zip.writeEntry(
                "xl/_rels/workbook.xml.rels",
                """
                <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
                <Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
                  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>
                  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
                </Relationships>
                """.trimIndent(),
            )
            zip.writeEntry("xl/styles.xml", stylesXml())
            zip.writeEntry("xl/worksheets/sheet1.xml", sheetXml(ride, gpsPointCount))
            zip.writeEntry(
                "docProps/core.xml",
                """
                <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
                <cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
                    xmlns:dc="http://purl.org/dc/elements/1.1/"
                    xmlns:dcterms="http://purl.org/dc/terms/"
                    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
                  <dc:creator>DIY Bike Computer</dc:creator>
                  <dc:title>Ride summary</dc:title>
                  <dcterms:created xsi:type="dcterms:W3CDTF">${Instant.now()}</dcterms:created>
                </cp:coreProperties>
                """.trimIndent(),
            )
            zip.writeEntry(
                "docProps/app.xml",
                """
                <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
                <Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties">
                  <Application>DIY Bike Computer</Application>
                </Properties>
                """.trimIndent(),
            )
        }
    }

    private fun sheetXml(ride: RideEntity, gpsPointCount: Int): String {
        val rows = listOf(
            listOf("Ride", ride.rideId.substringAfterLast(':'), ""),
            listOf("Started", ride.startedAtUtcMs?.let(::formatTime) ?: "Unavailable", ""),
            listOf("Finished", ride.finishedAtUtcMs?.let(::formatTime) ?: "Unavailable", ""),
            listOf("Distance", "%.2f".format(java.util.Locale.US, ride.distanceM / 1000.0), "km"),
            listOf("Moving time", formatDuration(ride.movingTimeMs), ""),
            listOf("Elapsed time", formatDuration(ride.elapsedTimeMs), ""),
            listOf(
                "Average moving speed",
                "%.1f".format(java.util.Locale.US, ride.averageSpeedKmh),
                "km/h",
            ),
            listOf(
                "Maximum speed",
                "%.1f".format(java.util.Locale.US, ride.maxSpeedKmh),
                "km/h",
            ),
            listOf("Data integrity", if (ride.synced) "Verified" else "Incomplete", ""),
            listOf("GPS points", gpsPointCount.toString(), ""),
        )
        val body = buildString {
            append(row(1, listOf("DIY Bike Computer · Ride summary"), 1))
            append(row(3, listOf("Metric", "Value", "Unit"), 2))
            rows.forEachIndexed { index, values -> append(row(index + 4, values, 0)) }
        }
        return """
            <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
            <worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
              <cols>
                <col min="1" max="1" width="27" customWidth="1"/>
                <col min="2" max="2" width="24" customWidth="1"/>
                <col min="3" max="3" width="12" customWidth="1"/>
              </cols>
              <sheetData>$body</sheetData>
              <mergeCells count="1"><mergeCell ref="A1:C1"/></mergeCells>
              <autoFilter ref="A3:C13"/>
            </worksheet>
        """.trimIndent()
    }

    private fun row(index: Int, values: List<String>, style: Int): String = buildString {
        append("""<row r="$index">""")
        values.forEachIndexed { column, value ->
            val reference = "${('A'.code + column).toChar()}$index"
            append(
                """<c r="$reference" s="$style" t="inlineStr"><is><t>""" +
                    escapeXml(value) + "</t></is></c>",
            )
        }
        append("</row>")
    }

    private fun stylesXml(): String =
        """
        <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
        <styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
          <fonts count="3">
            <font><sz val="11"/><name val="Calibri"/></font>
            <font><b/><sz val="18"/><color rgb="FF176B63"/><name val="Calibri"/></font>
            <font><b/><sz val="11"/><color rgb="FFFFFFFF"/><name val="Calibri"/></font>
          </fonts>
          <fills count="3">
            <fill><patternFill patternType="none"/></fill>
            <fill><patternFill patternType="gray125"/></fill>
            <fill><patternFill patternType="solid"><fgColor rgb="FF176B63"/><bgColor indexed="64"/></patternFill></fill>
          </fills>
          <borders count="1"><border/></borders>
          <cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>
          <cellXfs count="3">
            <xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>
            <xf numFmtId="0" fontId="1" fillId="0" borderId="0" xfId="0"/>
            <xf numFmtId="0" fontId="2" fillId="2" borderId="0" xfId="0"/>
          </cellXfs>
          <cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles>
        </styleSheet>
        """.trimIndent()

    private fun formatTime(milliseconds: Long): String =
        dateTimeFormatter.format(Instant.ofEpochMilli(milliseconds))

    private fun formatDuration(milliseconds: Long): String {
        val totalSeconds = milliseconds / 1000
        return "%02d:%02d:%02d".format(
            totalSeconds / 3600,
            (totalSeconds / 60) % 60,
            totalSeconds % 60,
        )
    }

    private fun escapeXml(value: String): String = value
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace("\"", "&quot;")
        .replace("'", "&apos;")

    private fun ZipOutputStream.writeEntry(name: String, content: String) {
        putNextEntry(ZipEntry(name))
        write(content.toByteArray(Charsets.UTF_8))
        closeEntry()
    }
}
