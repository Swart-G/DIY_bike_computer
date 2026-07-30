# Android companion architecture

Target: Kotlin, Jetpack Compose, Material 3, Room, coroutines/Flow, `minSdk 26`.

## Development APK

Run `android/gradlew testDebugUnitTest assembleDebug`. The checked development
artifact is copied to `apk/DIY-Bike-Computer-2.0.0-dev-debug.apk`; verify it
against `apk/SHA256SUMS` before sideloading.

## Modules and ownership

```text
ble/         GATT client, protocol codec, connection service
device/      known device and association repository
rides/       Room entities/DAO, importer and exports
location/    foreground ride location service and GPS repository
media/       notification-listener MediaSession bridge
navigation/  NavigationProvider abstraction and normalized state
settings/    validated device settings repository
ui/          onboarding, home, live, history, device, sync, setup, diagnostics
```

The app mirrors ESP ride state; it does not create a competing ride state machine.
Repositories expose immutable `StateFlow`/`Flow` models to Compose. The Compose shell
has three primary destinations: Home, History and Settings. Home is connection-focused
until a link is ready, then replaces that content with battery, speed, distance,
average/maximum speed, ride timing, motion and storage telemetry. History includes
filters, aggregate metrics, ride detail, speed chart and optional GPS route. Settings
contains remembered devices, the editable firmware Ride/Speed LED settings, phone-side
location/media controls and read-only device information.

## Connection lifecycle

Companion-device association is used where supported. A known Bluetooth address is
reconnected directly with bounded `1, 2, 4, 8, 15, 30 s` backoff after ESP reboot,
temporary range loss or Bluetooth disruption; app reopen also reconnects. Pairing scan
uses the system Companion Device chooser and is never permanent.

The Android repository keeps multiple remembered bike computers, one selected reconnect
target and the Companion Device association ID where Android exposes it. Forget requires
confirmation, closes an active GATT link, removes only that app entry and asks Android
to disassociate it. Existing local ride history is deliberately retained.

Android states:

```text
UNPAIRED -> CONNECTING -> INITIALIZING -> READY
                                          ^             |
                                          + RECONNECTING+
READY -> DISCONNECTED
any state -> ERROR
```

GPS recording runs in a foreground location service. BLE stays process-local; during an
active GPS ride the foreground service keeps the process alive, while loss of the
Android process still cannot affect the ESP ride.

## Permissions

Permissions are requested with in-context explanations:

- Nearby Devices during pairing;
- foreground/background location only when GPS Assist is enabled;
- notification access only during Media setup.

Denial disables only the corresponding companion feature.

## Data

Logical Room entities: `Device`, `Ride`, `RideFile`, `GpsPoint`, `RideEvent`. Large raw
CSV samples may remain as verified app-private files with indexed summaries instead of
one Room row per sample. Imports are transactional and idempotent.

Full CSV export is available for a synced ride. It preserves every firmware
`samples.csv` column and appends `timestamp_utc_ms`, latitude, longitude, altitude,
accuracy and diagnostic GPS speed from the nearest phone point within five seconds.
Missing location stays blank and never fabricates a coordinate. Brief XLSX export
contains only the ride summary (date, distance, moving/elapsed time, average/maximum
speed, integrity and GPS point count), not the sample stream. GPX is offered only when
GPS points exist. All exports use Android storage access APIs.

## GPS Assist

`RIDE_STARTED` starts/attaches a foreground location session keyed by ESP `ride_id`;
`RIDE_FINISHED` ends it. Reconnect to the same active `ride_id` resumes that session and
may leave an explicit gap. Hall-derived speed/distance remain the displayed primary
values. Disabling location immediately stops the foreground service.

## Media and navigation

Media uses direct `MediaController.TransportControls` for active system MediaSessions
obtained through notification-listener access. The user may pin an exact player; in
Auto mode the playing session is preferred. A pinned but inactive player remains
unavailable instead of silently controlling another source. Bounded title/artist/player
strings and supported actions are sent to the ESP, and ESP actions are executed only
when supported.

`NavigationProvider` exposes normalized maneuver state independent of a map SDK.
Navigation is experimental and feature-gated; provider absence leaves every other
feature operational.
