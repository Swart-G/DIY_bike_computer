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

GPS forwarding runs in a foreground location service. A separate `connectedDevice`
foreground service keeps the paired BLE GATT/reconnect loop alive while the app UI is
backgrounded and shows persistent connection state. Loss of the Android process still
cannot affect the ESP ride.

## Permissions

Permissions are requested with in-context explanations:

- Nearby Devices during pairing;
- notification permission for visible background BLE connection status on Android 13+;
- approximate and precise location together only when GPS Assist is enabled;
- background location in a second step: a runtime prompt on Android 10 or the app
  permission page with `Allow all the time` on Android 11+;
- notification access only during Media setup.

Background location is required because the ESP may start a ride through BLE while the
app activity is not visible. Denial or later revocation disables only GPS Assist and
stops its service without affecting ride recording on the ESP.

## Data

Logical Room entities are `Device`, `Ride`, `RideFile`, `RideSample` and `RideEvent`.
The legacy `GpsPoint` table remains read-only for already installed format-v1 history;
new phone fixes are never inserted into it. Large raw CSV samples remain as verified
app-private copies of the device-owned ride files with indexed non-location summaries.
Imports are transactional and idempotent.

Full CSV export is available for a synced ride. For format v2 it copies the verified
device `samples.csv`, which already contains timestamp, latitude, longitude, altitude,
accuracy, GPS speed and fix age. Missing location stays blank and never fabricates a
coordinate. Brief XLSX and GPX parse those columns transiently without inserting them
into Room; repeated timestamps are de-duplicated. All exports use Android storage
access APIs. Legacy format-v1 export may still read previously stored legacy points.

## GPS Assist

`RIDE_STARTED` starts/attaches a foreground location session keyed by the numeric ESP
`ride_id`; `RIDE_FINISHED` ends it. The service requests both GPS and network fixes,
rejects cached fixes older than ten seconds and encodes valid values into privileged
33-byte BLE `LocationFix` frames. There is no local point insert, disk retry queue or
offline fallback: if the bike is disconnected or the BLE queue is busy, that fix is
discarded and the next system fix is tried. Reconnect to the same active `ride_id`
resumes forwarding and may therefore leave an explicit gap. Process restart restores
only the active numeric ride ID, never a coordinate. Hall-derived speed/distance remain
authoritative. Settings and the foreground notification distinguish ready,
waiting-for-fix, sending and disconnected/discarded states.

## Media and navigation

Media uses direct `MediaController.TransportControls` for active system MediaSessions
obtained through notification-listener access. The user may pin an exact player; in
Auto mode the playing session is preferred. A pinned but inactive player remains
unavailable instead of silently controlling another source. Bounded title/artist/player
strings and supported actions are sent to the ESP, and ESP actions are executed only
when supported.

The firmware exposes the Media ride page from the negotiated media capability, even
before a player session exists. In that state it shows a prompt to start playback or
grant notification access. The companion rebinds its notification listener on resume
and periodically retries paused as well as playing state so a busy BLE queue cannot
permanently hide the page.

`NavigationProvider` exposes normalized maneuver state independent of a map SDK.
Navigation is experimental and feature-gated; provider absence leaves every other
feature operational.
