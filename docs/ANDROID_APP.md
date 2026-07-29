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
Repositories expose immutable `StateFlow`/`Flow` models to Compose. The current Compose
shell includes Home/Live, History, Ride Detail, speed chart, GPS route, Device settings,
sync/diagnostics and contextual GPS/Media setup. CSV and GPX exports use the Storage
Access Framework.

## Connection lifecycle

Companion-device association is used where supported. A known Bluetooth address is
reconnected directly with bounded `1, 2, 4, 8, 15, 30 s` backoff after ESP reboot,
temporary range loss or Bluetooth disruption; app reopen also reconnects. Pairing scan
uses the system Companion Device chooser and is never permanent.

Android states:

```text
UNPAIRED -> SEARCHING -> CONNECTING -> INITIALIZING -> READY
                                          ^             |
                                          + RECONNECTING+
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

CSV export is always available for a synced ride. GPX export is offered only when GPS
points exist. Exports use Android storage access APIs.

## GPS Assist

`RIDE_STARTED` starts/attaches a foreground location session keyed by ESP `ride_id`;
`RIDE_FINISHED` ends it. Reconnect to the same active `ride_id` resumes that session and
may leave an explicit gap. Hall-derived speed/distance remain the displayed primary
values.

## Media and navigation

Media reads the active system MediaSession through notification-listener access and
sends bounded title/artist/player strings plus supported actions. ESP actions are
executed only when supported.

`NavigationProvider` exposes normalized maneuver state independent of a map SDK.
Navigation is experimental and feature-gated; provider absence leaves every other
feature operational.
