# DIY Bike Computer 2.0 architecture

Status: architecture contract for firmware `2.1.1`.

## Product boundary

The ESP32-S3 remains the autonomous ride computer. Hall pulses are the authority for
speed and distance; `RideStateMachine` is the authority for ride state and statistics;
`StorageManager`/`RideLogger` own persistent ride data. Android is an optional companion
for absolute time, GPS geometry, media, navigation, archive and convenient settings.

Loss or failure of BLE, Android, GPS, media or navigation must not stop sensor sampling,
ride state updates, logging, recovery, battery updates or the local UI.

## Existing code and safe extension points

| Existing module | 2.0 responsibility |
|---|---|
| `HallSensor` | Keep ISR minimal; expose accepted/rejected pulse snapshots only. |
| `SpeedCalculator` | Preserve raw, filtered and display speed as separate values. |
| `RideStateMachine` | Remains the only ride-state/statistics authority; add a distinct motion state for auto pause. |
| `StorageManager` | Remains SD owner/arbitrator and config/recovery backend. |
| `RideLogger` | Append-only samples/events and atomic metadata/summary updates. |
| `RideRepository` | Safe ride enumeration/manifests and file-ID lookup for sync. |
| `DisplayManager` | TFT/framebuffer and fixed normal backlight. |
| `TouchManager` | FT6336 acquisition and coordinate transform; expose both controller points. |
| `UsbMassStorageManager` | Exclusive host ownership of SD until reboot. |
| `UiApp` | Composition/update loop only; routing, components and screens move to focused modules. |

No module may duplicate these authorities.

## Firmware structure

```text
src/
  phone/
    BleTransport
    BikeProtocol
    PhoneLinkManager
    PhoneState
  sync/
    RideSyncManager
  media/
    MediaState
  navigation/
    NavigationState
  time/
    ClockManager
  rain/
    RainLockManager
  ui/
    UiApp
    UiRouter
    components/
    screens/
```

Dependencies point inward: transport parses bytes into protocol messages; managers own
state; UI reads state and emits user intent. UI never owns BLE, sync, media, navigation
or Rain Lock state.

## Main-loop scheduling

All work is bounded and nonblocking:

1. capture Hall snapshot and update speed/ride core;
2. update battery and logging/recovery;
3. read FT6336 and pass the complete frame through `RainLockManager`;
4. service BLE RX/TX and one sync step;
5. update UI model and transfer only dirty regions/screens.

Rain unlock animation may request 20–30 FPS while its overlay is open. Normal screens
use the configured UI cadence. BLE telemetry defaults to 2 Hz and is skipped/coalesced
under backpressure. File transfer performs at most one bounded chunk step per service
call.

## UI architecture

The source of truth is `bike_computer_v2_exact_sourcepack`:

- `source/layouts/*.json`: complete 480×320 geometry, composition and dynamic
  region declarations;
- `reference_png/`: 24-bit visual reference;
- `rgb565_expected/`: expected ST7796 RGB565 result;
- `source/esp32/generated/`: 60 RLE565 assets generated from the layouts;
- `source/esp32/include` and `source/esp32/src`: exact TFT renderer.

The supplied assets are compiled into `src/ui_exact` unchanged and remain in PROGMEM.
`ExactScreenRenderer` can draw a full baseline or restore a bounded region from the
RLE stream. Static mock values never become application state: Home, Ride, Phone,
History, Settings and diagnostics replace only their declared dynamic regions from
view models. `UiRouter` owns navigation and the current ride page. Reusable components
own status bars, buttons, cards, chips, progress and icons. Screen modules own layout.
Diagnostics → Display test → Next opens a no-overlay pixel-exact gallery of all 60
assets for real-panel comparison.

Normal ride updates reuse the PSRAM framebuffer and transfer only the live content band;
the exact baseline is restored only for that region before current values are drawn.
Rain animation similarly restores only its 360×186 overlay region. Header/footer are
transferred again only when their state changes. Every full screen and sprite uses
RGB565 `0x0861`; `TFT_BLACK` is reserved for deliberate dimming/test pixels.
Brightness remains readable from old format-1 configs for compatibility but is ignored
by the 2.0 user interface. The backlight uses the fixed normal hardware mode.

## Touch and Rain Lock

The FT6336 register path reads `TD_STATUS`, P1 and P2 in one I²C transaction and
preserves the existing first-point convenience API.
Each point contains valid/touch ID/raw/mapped coordinates.

```text
FT6336 -> TouchManager -> RainLockManager -> UiApp
```

When Rain Lock is active, no normal UI gesture or tap reaches `UiApp`. After the main
enable toast, an ordinary touch only shows a compact hint beside the Rain icon. One
point must remain in each radius-36 target centered at `(160,192)` and `(320,192)` for
a 2000 ms pre-hold before the unlock overlay opens. The overlay then starts a separate
continuous 3000 ms hold with ripple animation. Point ordering is irrelevant. Large
discontinuous movement, release, missing points or leaving either target closes the
overlay and resets both stages. Rain Lock is initialized OFF on every boot and is
never persisted.

## BLE and protocol

ESP is a BLE peripheral/GATT server; Android is the central/client. Bluetooth Classic is
not used. `BleTransport` owns GATT and byte queues. `BikeProtocol` owns framing, CRC and
payload validation. `PhoneLinkManager` owns connection, authentication, negotiated
protocol/capabilities and reconnect-visible state.

State:

```text
DISABLED -> ADVERTISING -> CONNECTED -> AUTHENTICATING -> READY
                                                        -> TRANSFERRING
any state ----------------------------------------------> ERROR
```

BLE connection alone is not authorization. Privileged messages require an encrypted,
bonded link and the stored association identity. Details are in `BLE_PROTOCOL.md`.

## Storage ownership and sync

`StorageManager` is the single arbiter:

- firmware owner: logging/repository/sync may use SD through bounded operations;
- USB owner: firmware FAT access and ride sync are blocked;
- no SD: ride core and live telemetry continue; sync reports an explicit error.

An active ride is never exposed through the file-transfer API. A sync request references
an enumerated `ride_id` and `file_id`, never a client-provided path. See
`RIDE_SYNC.md`.

## Time

`ClockManager` stores the last trusted Android epoch in milliseconds and the monotonic
`millis()` captured with it. `epochNow()` derives time from the delta and handles
`millis()` rollover. Reconnect resynchronizes. A ride created before time sync remains
valid and uses nullable absolute timestamps. New timestamp fields are additive and do
not change ride format version 1.

## Android

Android uses Kotlin, Compose Material 3, Room, coroutines and Flow, with `minSdk 26`.
The BLE connection manager retries a known bonded address with bounded exponential
backoff and never runs a permanent scan. GPS uses a foreground location service only
during an ESP-authoritative active ride, keeping the process alive for active route
recording. Media uses system MediaSession/MediaController. Navigation is behind
`NavigationProvider` and a feature flag.

See `ANDROID_APP.md`.

## Reliability and observability

Priority is Hall -> ride state -> logging/recovery -> UI -> companion features.
Subsystem logs use `RIDE`, `SENSOR`, `STORAGE`, `UI`, `RAIN`, `BLE`, `PROTO`, `SYNC`,
`MEDIA`, `NAV`, `GPS`. High-rate telemetry is not logged by default.

Every major phase must pass `pio run`; Android phases must pass
`android/gradlew assembleDebug`. Real-hardware checks are mandatory for touch mapping,
two-point Rain Lock, BLE reconnect, shared-SPI integrity and USB ownership.
