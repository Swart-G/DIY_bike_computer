# Production roadmap status

Completed in firmware 1.0.0:

- Fixed ST7796/FT6336/shared-SPI GPIO contract, PWM backlight and fault-tolerant boot.
- Hall ISR, plausibility filtering, raw/filtered/display speed and pulse-based distance.
- Full ride state machine and distinct elapsed, recording, moving, stopped and pause timings.
- Production `RideLogger`, atomic recovery, NVS checkpoint, SD-loss RAM buffer and summary/history.
- GPIO6 battery sampling, Li-Po SoC/trend and calibration persistence.
- GPIO48 RGB speed-trend indication plus an optional 2/5/10-second F1 Ride page with
  independent persistent tolerances.
- ST7796 health probing/framebuffer restore, FT6336 retry and dropped-scan tolerance,
  flicker-free spoked-wheel bicycle boot animation with bounded logs, runtime SD
  safe-first mount/preflight with bounded multi-attempt recovery, enlarged Back targets
  and a four-phone NVS registry.
- Settings/config validation, diagnostics, graph and USB MSC owner model.

Verification remains hardware-dependent: execute the complete manual plan after wiring.
Future ESP-side GPS scope requires actual GNSS hardware; version 2.0 route geometry
comes only from the optional Android companion and firmware never fabricates it.

Version 2.0 development:

- Phases 0–10 are software-complete and build-verified for firmware and Android:
  sourcepack UI, Rain Lock, BLE/protocol/pairing, telemetry/time, resumable ride sync,
  GPS/Room/history/export, MediaSession, experimental `NavigationProvider`, safe config
  sync and distinct Auto Pause motion state.
- Phase 11 remains a real-device acceptance gate: sourcepack visual comparison, FT6336
  two-point/water-like tests, BLE bonding/reconnect across phones, long ride/SD removal,
  USB ownership, GPS background behavior, media-player matrix and heap/performance.
- Navigation intentionally ships provider-unavailable until a concrete routing provider
  is selected; all other features remain operational.
- Firmware stays `2.1.0-dev` until every hardware gate in `docs/TEST_PLAN_V2.md` passes.
