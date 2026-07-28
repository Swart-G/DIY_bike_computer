# Production roadmap status

Completed in firmware 1.0.0:

- Fixed ST7796/FT6336/shared-SPI GPIO contract, PWM backlight and fault-tolerant boot.
- Hall ISR, plausibility filtering, raw/filtered/display speed and pulse-based distance.
- Full ride state machine and distinct elapsed, recording, moving, stopped and pause timings.
- Production `RideLogger`, atomic recovery, NVS checkpoint, SD-loss RAM buffer and summary/history.
- GPIO6 battery sampling, Li-Po SoC/trend and calibration persistence.
- Settings/config validation, diagnostics, graph and USB MSC owner model.

Verification remains hardware-dependent: execute the complete manual plan after wiring. Future scope may add GPS only if actual GNSS hardware is installed; current firmware intentionally does not manufacture route data.
