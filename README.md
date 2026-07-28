# DIY Bike Computer

Production firmware for an ESP32-S3-N16R8 bike computer with ST7796 480×320 TFT, FT6336 touch, SPI microSD, native USB Mass Storage, wired Hall sensor and 1S Li-Po monitor.

Features: accurate Hall speed with deceleration-to-stop, distance from pulses, Start/Pause/Resume/confirmed Finish, elapsed/recording/moving/stopped statistics, auto-scaled 60-second configurable graph, versioned SD ride logs, atomic SD+NVS recovery, ride history, safe deletion, config persistence, GPIO6 battery telemetry, diagnostics and a strict USB/firmware SD ownership model.

Build:

```bash
pio run
pio run -t upload
pio device monitor
```

The complete pin contract is in [docs/HARDWARE.md](docs/HARDWARE.md), file contract in [docs/LOG_FORMAT.md](docs/LOG_FORMAT.md), and real-device procedure in [docs/TEST_PLAN.md](docs/TEST_PLAN.md). Native USB pins GPIO19/20 must never be used as general GPIO. There is no GPS hardware: logs contain bike telemetry, not fabricated locations or GPX tracks.
