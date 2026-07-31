# Production roadmap status

Completed in firmware 1.0.0:

- Fixed ST7796/FT6336/shared-SPI GPIO contract, PWM backlight and fault-tolerant boot.
- Hall ISR, plausibility filtering, raw/filtered/display speed and pulse-based distance.
- Full ride state machine and distinct elapsed, recording, moving, stopped and pause timings.
- Production `RideLogger`, atomic recovery, NVS checkpoint, SD-loss RAM buffer and summary/history.
- GPIO6 battery sampling, Li-Po SoC/trend and calibration persistence.
- GPIO48 RGB speed-trend indication plus an optional 2/5/10-second F1 Ride page with
  independent persistent tolerances.
- Proven 1.0 ST7796/SD shared-SPI ownership without background panel probes or automatic
  bus/card reinitialization, FT6336 retry and dropped-scan tolerance, flicker-free
  spoked-wheel bicycle boot animation with bounded logs, enlarged Back targets and a
  four-phone NVS registry.
- Settings/config validation, diagnostics, graph and USB MSC owner model.

Verification remains hardware-dependent: execute the complete manual plan after wiring.
Future ESP-side GPS scope requires actual GNSS hardware; version 2.0 route geometry
comes only from the optional Android companion and firmware never fabricates it.

Version 2.0 development:

- Phases 0–10 are software-complete and build-verified for firmware and Android:
  sourcepack UI, Rain Lock, BLE/protocol/pairing, telemetry/time, resumable ride sync,
  GPS/history/export, MediaSession, experimental `NavigationProvider`, safe config
  sync and distinct Auto Pause motion state.
- Phase 11 remains a real-device acceptance gate: sourcepack visual comparison, FT6336
  two-point/water-like tests, BLE bonding/reconnect across phones, long ride/SD removal,
  USB ownership, GPS background behavior, media-player matrix and heap/performance.
- Navigation intentionally ships provider-unavailable until a concrete routing provider
  is selected; all other features remain operational.
- Firmware release `2.1.1` incorporates the completed hardware-driven SD/display
  stability rollback; remaining acceptance checks stay documented in `docs/TEST_PLAN_V2.md`.
- The 2.1.1 maintenance audit bounds live framebuffer transfers, validates persisted
  numeric configuration, hardens BLE framing/write timeouts and ride imports, and
  publishes the Android companion under a persistent release-signing certificate.
- Release `2.2.0` moves live phone location ownership to the bike computer: Android
  forwards validated fixes without a Room insert, firmware accepts only the matching
  active ride and stores fresh fixes in format-v2 `samples.csv`. Legacy v1 rides remain
  readable and the Dev stream exposes end-to-end location counters.
