# AGENTS.md — DIY Bike Computer production rules

Before changing firmware read `README.md`, `docs/HARDWARE.md`, `docs/FIRMWARE_SPEC.md`, `docs/LOG_FORMAT.md`, `docs/TEST_PLAN.md` and `PLAN.md`. Hardware contract wins for GPIO; firmware spec wins for behaviour.

## Platform and fixed GPIO

PlatformIO + Arduino on ESP32-S3-N16R8; display is ST7796, touch FT6336, SD uses shared SPI and USB is native ESP32-S3. All GPIO identifiers are centralised in `src/config/hardware_config.h`. Never alter them without explicit user authorization:

```text
GPIO4 Hall; GPIO6 battery ADC; GPIO8 SDA; GPIO9 DC; GPIO10 LCD CS;
GPIO11 MOSI; GPIO12 SCK; GPIO13 MISO; GPIO14 LCD RST; GPIO15 SD CS;
GPIO16 touch INT; GPIO17 touch RST; GPIO18 SCL; GPIO19 USB D−;
GPIO20 USB D+; GPIO47 backlight.
```

GPIO19/20 are never general GPIO. TFT and SD must never have active CS simultaneously. Prefer stable SPI frequency to throughput and never auto-format a card after a mount failure.

## Production invariants

- No SD, Hall, touch or invalid JSON may create boot loops. No-SD mode continues live speed/ride UI without persistent logging.
- Hall ISR does only timestamp/counter/minimal filtering; no display, SD, allocation or serial work.
- Distance is pulse-derived, not integrated display speed. Preserve raw, filtered and display speed separately.
- Ride transitions are IDLE/RIDING/PAUSED/FINISHED. Do not count paused pulses or time as ride distance/moving time. Never auto-resume a recovered ride.
- SD CSV is append-only; JSON uses a temp file and rename. Keep NVS recovery/config copies small and periodic, never samples.
- Exactly one owner accesses SD: firmware or USB host. Close/checkpoint before MSC; after MSC safe eject, reboot is the supported exit.
- Battery uses GPIO6 and the documented high-impedance divider. Sampling must remain nonblocking and use calibration/filtering; do not invent charge GPIO or GPS coordinates.

Keep modules focused (`StorageManager`, `RideLogger`, `RideRepository`, speed, battery, UI). Avoid String allocation in hot loop/ISR. Validate with `pio run`, inspect the diff, update documentation and state a practical real-hardware check.
