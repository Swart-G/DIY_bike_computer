# DIY Bike Computer 2.0 implementation checklist

This is the phase gate. A phase is complete only when its builds, automated checks,
documentation and stated hardware check are complete.

## Phase 0 — audit and contracts

- [x] Read firmware, platform config and production documents.
- [x] Inventory fixed GPIO and shared-SPI/USB ownership rules.
- [x] Inspect all 60 sourcepack screens, manifest, markup and runtime scaffold.
- [x] Confirm current FT6336 layer reads only P1 and locate P2 register extension.
- [x] Define architecture, BLE protocol, sync contract, Android architecture and V2 tests.
- [x] Confirm baseline `pio run`.

## Phase 1 — UI 2.0

- [x] Apply sourcepack RGB565 tokens and 480×320 geometry.
- [x] Add `UiRouter`, shared components and unified `IconRenderer`.
- [x] Implement Home with Start, History, Phone and Settings only.
- [x] Implement Ride Speed, Stats and Graph with dynamic values.
- [x] Implement IDLE / RIDING / PAUSED control geometry and Finish confirmation.
- [x] Implement History/list/detail, Settings hierarchy, Diagnostics and Speed LED.
- [x] Remove all user-facing brightness controls while reading old config safely.
- [x] Reuse the framebuffer and transfer only the live Ride content band at telemetry cadence.
- [x] Import all 60 exact RLE565 assets into PROGMEM and preserve them byte-for-byte.
- [x] Add bounded baseline-region restore for Ride/Rain runtime redraws.
- [x] Add the pixel-exact Display diagnostic gallery.
- [ ] Compare practical screen renders and touch targets on hardware (`pio run` passes).

## Phase 2 — Rain Lock

- [x] Extend `TouchManager` to two points in one bounded FT6336 read.
- [x] Add `RainLockManager` state machine; boot state always OFF.
- [x] Globally intercept all locked touches before `UiApp`.
- [x] Add small shared rain icon and ~44×36 hit target on ride pages.
- [x] Add enable toast and locked idle behavior.
- [x] Implement `(160,192)` / `(320,192)`, radius 36, order-independent validation.
- [x] Implement continuous 3000 ms hold, discontinuity checks and immediate reset.
- [x] Add runtime ripples, progress text/bar, timeout and success state.
- [x] Compile embedded state-transition/protocol tests with `pio test --without-uploading --without-testing`.
- [ ] Run embedded tests and the full FT6336 checklist on hardware.

## Phase 3 — BLE foundation

- [x] Add bounded codec/parser and shared protocol constants.
- [x] Add GATT service, Device Info, RX and TX.
- [x] Add runtime passkey, secure pairing/bond and NVS association.
- [x] Add HELLO/version/capabilities negotiation and authorization gates.
- [x] Add connection state model and diagnostics.
- [x] Scaffold Android Compose/Room/coroutines app and GATT client.
- [x] Generate C++ vectors and load Kotlin vectors from the same canonical JSON.

## Phase 4 — live telemetry

- [x] Publish 2 Hz telemetry without Serial spam.
- [x] Publish immediate ride, SD, battery and Rain Lock events.
- [x] Build Android Live Ride from ESP-authoritative state.

## Phase 5 — time

- [x] Add monotonic `ClockManager`.
- [x] Resync after every READY transition.
- [x] Add nullable backward-compatible UTC metadata fields.

## Phase 6 — ride sync

- [x] Add safe finished-ride manifests and fixed file IDs.
- [x] Implement MTU-sized stop-and-wait chunk transfer.
- [x] Implement offset resume, frame CRC16 and file CRC32.
- [x] Respect active ride and USB ownership.
- [x] Add Room importer, deduplication and device-scoped stable IDs.

## Phase 7 — Android GPS

- [x] Add contextual permissions and foreground location service.
- [x] Attach sessions by ESP ride ID and preserve gaps.
- [x] Add route map and GPX; retain Hall authority.
- [x] Add full telemetry+location CSV and summary-only XLSX exports.

## Phase 8 — media

- [x] Add MediaSession bridge, bounded metadata and supported-actions mask.
- [x] Add ESP Media state/screen and safe controls.
- [x] Add user-selectable player pinning with safe Auto fallback.

## Phase 9 — experimental navigation

- [x] Add `NavigationProvider` abstraction and normalized lifecycle/maneuvers.
- [x] Add NAV_STATE codec and ESP turn-only screen.
- [x] Gate unavailable provider without affecting other features.

## Phase 10 — settings sync and auto pause

- [x] Add distinct MotionState and auto-pause timing.
- [x] Add CONFIG_GET/SET/RESULT with ESP validation.
- [x] Exclude all hardware/GPIO mapping.
- [x] Add Android Ride and Speed LED settings plus read-only device information.
- [x] Add remembered bike-computer list, switching and confirmed forgetting.
- [x] Replace Android Status/Bike navigation with connection-aware Home and Settings.

## Phase 11 — production polish

- [ ] Complete `TEST_PLAN.md` and `TEST_PLAN_V2.md` on hardware.
- [ ] Long ride, reconnect, SD removal, USB conflict and heap/performance tests.
- [ ] Check backward-compatible config and v1 rides.
- [x] Review software diff/docs and release version `2.1.1` after the hardware-driven
  SD/display stability rollback; retain remaining acceptance gates in the test plan.

## Last software gate — 2026-07-28

- [x] `pio run` with all exact assets — RAM 75,028 / 327,680 bytes; Flash
  2,525,173 / 6,553,600 bytes.
- [x] Embedded tests compile with
  `pio test -e esp32s3-n16r8 --without-uploading --without-testing`.
- [x] `./gradlew testDebugUnitTest assembleDebug` — four canonical protocol tests pass.
- [x] `git diff --check`; fixed GPIO contract files have no diff.
- [ ] Hardware upload and all real-device acceptance procedures remain required.

## Android UX/export gate — 2026-07-30

- [x] `pio run` — RAM 79,548 / 327,680 bytes; Flash
  2,559,557 / 6,553,600 bytes.
- [x] `./gradlew lintDebug testDebugUnitTest assembleDebug` with JDK 21.
- [x] XLSX package/content unit test and `git diff --check`.
- [ ] Verify two physical bike computers, Companion Device disassociation, background
  GPS/CSV alignment and pinned media control on representative Android versions.
