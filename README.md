# DIY Bike Computer 2.1.1

Firmware and Android companion project for an ESP32-S3-N16R8 bike computer with
ST7796 480×320 TFT, FT6336 touch, SPI microSD, native USB Mass Storage, wired Hall
sensor and 1S Li-Po monitor.

Version `2.1.1` is the current software release. The autonomous Hall speed/distance, ride
state, logging/recovery, history, battery, diagnostics and strict USB/firmware SD
ownership remain the reliability core. Version 2.0 adds the
`bike_computer_v2_exact_sourcepack` UI, global two-point Rain Lock,
Auto Pause, versioned BLE, resumable ride synchronization, Android GPS/Room
history/export, system MediaSession controls, safe device settings and feature-gated
experimental navigation. The built-in GPIO48 RGB LED mirrors the two-second segment of
an optional F1-style 2/5/10-second speed-trend page; every segment has a persistent
tolerance. The ST7796 and SD use the proven 1.0 shared-SPI ownership model without
background panel probes or automatic bus/card reinitialization, while the animated boot
screen shows bounded stage logs. Frequently changing Ride and diagnostics data is
committed as bounded framebuffer regions, reducing shared-SPI traffic and avoiding
whole-screen flicker. Up to four
companion phones are remembered and listed on-device. All 60 supplied RGB565/RLE
screens are linked as a pixel-exact regression baseline; production screens replace only their declared
dynamic regions with runtime data. The optional Android companion remembers multiple
bike computers, provides a connection-aware live Home screen, mirrors Ride and Speed
LED settings, records phone GPS during active rides, lets the user pin the controlled
system media player, and exports full telemetry+location CSV, brief XLSX and GPX.

The common status header includes battery percentage plus a learned, smoothed remaining
runtime estimate; it intentionally shows `~ --` until a meaningful discharge history
has been observed.

Build:

```bash
pio run
pio run -t upload
pio device monitor
```

Android debug build:

```bash
cd android
./gradlew testDebugUnitTest assembleDebug
```

Android release builds require the persistent signing identity described in
[`android/signing/README.md`](android/signing/README.md). The distributable signed APK
and its checksum are in [`apk/`](apk/).

The complete pin contract is in [docs/HARDWARE.md](docs/HARDWARE.md), version-2
architecture in [docs/V2_ARCHITECTURE.md](docs/V2_ARCHITECTURE.md), file contract in
[docs/LOG_FORMAT.md](docs/LOG_FORMAT.md), and real-device procedures in
[docs/TEST_PLAN.md](docs/TEST_PLAN.md) and
[docs/TEST_PLAN_V2.md](docs/TEST_PLAN_V2.md). Native USB pins GPIO19/20 must never be
used as general GPIO. The ESP has no GPS hardware; route geometry is optional Android
companion data and is never fabricated by firmware.
