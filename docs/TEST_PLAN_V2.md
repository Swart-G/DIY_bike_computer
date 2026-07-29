# DIY Bike Computer 2.0 test plan

Run the existing `TEST_PLAN.md` in full; version 2.0 adds the following regressions.
Record firmware/app versions, phone model/Android version, SD model, negotiated MTU and
battery voltage.

## Build and autonomous regression

- `pio run` succeeds after every firmware phase.
- `android/gradlew testDebugUnitTest assembleDebug` succeeds after Android foundation exists.
- `pio test -e esp32s3-n16r8 --without-uploading --without-testing` compiles
  embedded protocol/Rain Lock tests; run them on the connected target before release.
- With Bluetooth disabled and phone absent: boot, Hall, speed, distance,
  Start/Pause/Resume/Finish, logging, recovery, history, battery, settings,
  diagnostics and USB MSC all pass.
- Inspect heap at boot and after a three-hour ride; no unbounded decline.

## UI 2.0

- Verify 480×320 orientation and sourcepack geometry/colors for Home, Ride Speed/Stats/
  Graph, History, Settings, Diagnostics and Speed LED.
- Run `bike_computer_v2_exact_sourcepack/tools/verify_esp_assets.py`; all 60 assets
  must report zero RGB565 mismatches.
- Open Diagnostics → Display test → Next. In exact gallery, tap the left/right third
  to browse all 60 source screens and the center third to return. Compare against
  `rgb565_expected/` without overlays or scaling.
- Inspect the complete background for a black/green seam: every normal clear must be
  `0x0861`; pure black is valid only in the K test patch and deliberate dim overlays.
- Confirm runtime speed, distance, time, ride count, pairing code, battery, storage,
  media and navigation replace the mock values shown in the reference PNGs.
- Home contains only Start Ride, History, Phone and Settings.
- Connect and disconnect the companion while Home is visible. Its phone icon and
  `Phone connected`/`Phone not connected` subtitle must change together without
  reopening the screen.
- On the connected Phone screen, test both `Bike Computer Android` and a peer name
  wider than the content area. The name must remain on its own row (using the
  smaller fallback font when needed) and must not overlap the `Connected` status.
- Inspect the Home heading at full size: the descender in `Ready to ride` must
  remain intact after the phone-status runtime patch.
- Settings menu/display labels use the native runtime font and remain sharp;
  no value/chevron overlap is visible.
- Boolean settings use inline toggles. Every editable numeric summary row opens a
  dedicated `− / + / Save` screen; Back discards its draft and Save persists it.
  Read-only Display/System rows have no chevron.
- History detail exposes distinct Delete, USB Storage and Back controls.
  Delete opens a confirmation modal, Cancel preserves the ride, and only the
  confirmed Delete removes it. Each control must respond across its full
  finger-sized hit area.
- Put an additional nonstandard file in a finished ride folder, confirm
  deletion, and verify the complete enumerated folder disappears and the
  refreshed History list no longer contains the ride. A failure must be shown
  in the History footer instead of silently retaining the row.
- With at least four rides, swipe History vertically in both directions.
  The scrollbar and `first-last of total` footer must follow the offset, and
  tapping a visible row must open that exact ride rather than the unscrolled
  index.
- Battery test shows Cal −, Cal +, Save and Back as four non-overlapping
  controls, with no sourcepack button visible underneath.
- No Brightness label, percentage, slider or editor exists; an old config containing
  `display_brightness_percent` still loads.
- In the Android companion, the four primary destinations use a Material 3 bottom
  navigation bar with icons, labels and a persistent selected state. Opening a ride
  detail and choosing any destination closes the detail and navigates directly.
- Android History shows aggregate distance, ride count and moving time, followed by
  per-ride distance, average speed, maximum speed, moving time and sync integrity.
  Ride detail shows a summary hero, four metric cards, a readable filled speed chart,
  optional GPS route, and enabled export actions only when their data is available.
- RIDING shows Pause + Finish; PAUSED shows Resume + Finish; Finish always confirms.
- Every Ride page exposes the Settings icon at the left side of the status bar.
  Returning from Settings restores the
  same active ride/page. During RIDING or manual PAUSED, Ride settings,
  Speed LED, Diagnostics and USB Storage are visibly locked and tapping them shows
  `Cannot change settings during a ride`; Display, Phone and System status
  remain readable.
- Exercise live speeds on both sides of 10 km/h (for example 12.3 → 9.8 →
  7.0). No remnants of the wider previous speed glyph may remain around the
  large digits.
- Swipe requires a clear horizontal movement and rejects excessive vertical motion or
  duration. No major flicker during live updates.

## Rain Lock

Test button hit area as well as visible icon. Verify:

- tap enables lock and shows a nonblocking enable toast;
- speed, distance, timers, logging, recovery, battery, BLE telemetry, GPS/media/nav
  updates continue;
- Pause/Resume/Finish, swipe, menu, history, settings, media and navigation taps do not
  reach normal handlers;
- after the enable toast, an ordinary locked touch performs no original action
  and shows only the compact hint immediately left of the Rain icon;
- one finger, two fingers outside, left only and right only do not start progress;
- correct left+right placement must remain valid for a 2.0-second pre-hold
  before the timer/ripple overlay opens; during this pre-hold only two small
  target dots are shown at `(160,192)` and `(320,192)`;
- after the overlay opens, 1.0, 2.0 and 2.9 seconds remain locked and a further
  continuous 3.0 seconds unlocks;
- release, leaving a radius-36 zone, missing P2 or invalid tracking during
  either stage immediately closes/resets the unlock flow;
- small movement inside each zone continues;
- point IDs may be left/right or right/left;
- rapid random and water-like contacts never unlock;
- ripples run at 20–30 FPS only while the overlay is open;
- success state lasts 500–800 ms and restores the prior ride page;
- reboot always starts Rain Lock OFF.

## BLE, pairing and reconnect

- boot -> advertising -> runtime passkey -> bond -> HELLO/ACK -> READY;
- wrong/expired passkey and unknown association remain unauthorized;
- disconnect/reconnect, ESP reboot, Bluetooth OFF/ON, out-of-range return, screen lock
  and app restart recover without aggressive scanning;
- Cancel from the passkey or `Connection lost` screen closes the current GATT link,
  clears the runtime code plus incomplete association/bond, returns to Phone unpaired
  and ends the phone PIN loop; a later Pair phone action starts a fresh pairing window;
- complete passkey pairing with at least one ride folder present on SD, confirm HELLO,
  time sync and automatic ride-list enumeration finish without a `loopTask`
  stack-canary reset, then restart the Android app and confirm the stored bond and
  application association reconnect without another PIN;
- after a successful pair, verify Android reports `BOND_BONDED`, force a real GATT/ACL
  disconnect, and confirm both app restart and ESP reboot reconnect using the stored
  LTK without opening the system PIN dialog; only Cancel pairing may remove the bond;
- remove the phone-side/controller bond while leaving the application association in
  NVS, reboot, and confirm firmware clears the stale association and exposes Pair phone
  instead of repeatedly requesting an unusable old PIN;
- invalid magic/version/length/CRC, partial and concatenated frames are contained;
- privileged commands are rejected on an unencrypted/unassociated link;
- telemetry runs at 2–5 Hz and immediate ride/SD/battery/Rain events are prompt.

## Sync

- sync 1, 20 and a large ride; verify manifest, every length/CRC and Room deduplication;
- disconnect around 10% and 73%, reconnect and confirm resume from acknowledged offset;
- retry a corrupt chunk and a whole-file CRC mismatch;
- import old v1 rides and generate stable IDs without modifying them;
- reject active ride files and path traversal;
- SD removal, read error and USB MSC return explicit errors while ride core continues;
- starting/resuming a ride defers or cancels bulk transfer.

## GPS

Test permission allowed/denied, GPS on/off, screen locked, BLE gap/reconnect and finish.
Ride remains valid in every failure case; reconnect attaches to the existing `ride_id`.
Map and GPX are hidden when no points exist. Compare primary distance against ESP Hall,
not Android GPS.

## Media

Test no notification access, no session, unsupported actions, metadata truncation,
play/pause/toggle/next/previous/seek and active-player switching with available players.

## Navigation

Test provider unavailable, route unavailable, every maneuver including UNKNOWN, empty
street, rerouting, BLE loss/reconnect and arrival. Disabling Navigation must not alter
the ride page sequence incorrectly.

## Settings and security

- valid CONFIG_GET/SET round trips for wheel circumference, threshold, auto pause,
  auto-pause delay, log interval and graph window;
- invalid/out-of-range values are rejected or safely clamped by ESP;
- GPIO/hardware mapping is not exposed;
- unauthorized config/download and arbitrary paths are rejected;
- USB ownership blocks all FAT-backed config and sync operations.
