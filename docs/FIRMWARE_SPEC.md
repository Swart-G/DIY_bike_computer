# Production firmware specification

The firmware is an autonomous bike computer: Hall speed acquisition, ST7796 touch UI, statistics, SD logging/recovery, battery telemetry, settings, diagnostics and USB MSC. It must boot without SD, Hall or FT6336.

Boot order: serial/build info; backlight/TFT splash; touch; SD; config (NVS defaults then valid SD config); Hall; battery; recovery; UI. No SD opens a Retry/Continue screen; Continue retains all live ride functions without persistent logging.

Speed uses accepted Hall intervals at microsecond resolution. One accepted pulse is `wheel_circumference_m / pulses_per_revolution`; pulses under configured debounce or exceeding maximum plausible speed are rejected. Raw, filtered and display speeds are separate. After the last interval expires, display speed is estimated from time since last pulse and naturally decays to zero below `stop_threshold_kmh` (default 3 km/h).

The built-in GPIO48 addressable RGB LED compares display speed with the sample from
two seconds earlier. A change above the configured stable tolerance is purple, a change
below the negative tolerance is red, and a change within the inclusive tolerance is
green. Before a full two-second history exists it is green. The Settings `Speed LED`
section replaces About and controls enable state, stable tolerance and brightness.
Boolean settings use an inline styled toggle. Numeric settings never change by tapping
their summary row: the row opens a dedicated `− / + / Save` editor. Back discards the
draft value, while Save validates, persists and then applies it. Read-only Display and
System values have no chevron.

Ride state is exactly `IDLE`, `RIDING`, `PAUSED`, `FINISHED`. A separate motion state
is `MOVING` or `AUTO_PAUSED`; Auto Pause never changes `RideState` and resumes
automatically when wheel motion returns. Distance comes only from accepted pulses while
RIDING. `elapsed` covers Start→Finish, `recording` only RIDING, `moving` only RIDING
before/while motion passes the configured Auto Pause timing, `pause` is
elapsed−recording, and `stopped` is recording−moving. Main UI AVG is distance/moving
time; it is never the arithmetic mean of samples. Finish requires confirmation, writes
FINISH/summary and only then clears recovery.

The 2.0 Home screen has Start Ride, History, Phone and Settings. Technical functions
(Diagnostics and USB Storage) live inside Settings. Ride pages are Speed, Stats
and auto-scaled Graph; Navigation and Media are added to the swipe sequence only when
their companion features are available. RIDING uses Pause + Finish and PAUSED uses
Resume + Finish. Finish always confirms. History reads only summary files; active rides
cannot be deleted.

Home derives both its phone icon and `Phone connected`/`Phone not connected` subtitle
from the same live `PhoneLinkManager::ready()` value. The connected Phone screen keeps
the peer display name and connection-state chip in separate bounded dynamic rows, with
a smaller fallback font for names that do not fit the available width.

Cancel on the Phone pairing or `Connection lost` screen ends pairing rather than
pausing reconnect: it closes the current GATT link, clears the runtime passkey, removes
the incomplete application association and BLE bond, and returns to the unpaired Phone
screen. Normal non-pairable advertising remains available for a later explicit pairing
attempt. Hall, ride state and logging continue unchanged.

The first authorized phone session may immediately request the SD ride manifest. Its
bounded protocol and 24-item repository scratch buffers are persistent members rather
than `loopTask` locals, leaving stack headroom for FatFs and JSON parsing. A newly
authenticated passkey session replaces any stale application association identifier;
an ordinary bonded reconnect must still present the exact stored identifier.

Runtime passkey configuration must leave BLE authentication in Secure Connections +
MITM + Bonding mode. The ESP32 Arduino `setStaticPIN()` helper changes the authentication
mode, so firmware reapplies the bonding/key-distribution parameters after every PIN
change. A normal disconnect, app restart or ESP reboot reuses the stored LTK without
showing another PIN; only explicit Cancel pairing removes it.
At boot, an application association without a matching controller bond is unusable and
is cleared automatically so the Phone screen returns to unpaired instead of entering a
repeating PIN/error loop.

The exact UI baseline is compiled from all 60 RLE565 assets in
`bike_computer_v2_exact_sourcepack`. Full-screen and sprite background is RGB565
`0x0861`. Runtime state replaces example text only inside dynamic regions; speed,
distance, ride timing, history, pairing codes, battery, storage, media and navigation
must never use the mock values embedded in the baseline. Normal Ride refresh restores
and transfers only the live content band. The Display diagnostic exposes the supplied
screens as a pixel-exact gallery for panel comparison.

Rain Lock globally intercepts touch after `TouchManager` and before `UiApp`. It never
blocks Hall, ride state/statistics, logging/recovery, battery, BLE or display updates.
Unlock requires both FT6336 contacts in the sourcepack target zones continuously for
3000 ms; release, invalid tracking or leaving a zone resets progress. Rain Lock is OFF
after every reboot.

Battery GPIO6/ADC1 is production-enabled. A nonblocking sampling state machine discards initial readings, takes seven samples, applies trimmed mean, divider and stored calibration factor, then low-pass filters for UI. SoC uses a 1S Li-Po interpolation table with percentage hysteresis. States include normal/low/critical plus charging/discharging/stable trend; USB is not treated as proof of charging.

USB MSC has a strict owner model. RIDING is refused; PAUSED is checkpointed and files are closed; IDLE/PAUSED SD is exposed to host. No FAT operation is permitted while active. Safe eject followed by reboot is the supported exit path.

Diagnostics: display primitives, both raw FT6336 touch points, paint, SD read/write/card
information, USB, Hall raw state and counters, battery raw/calibrated values, and system
info including BLE/protocol state. Runtime work is timer/state-machine driven; ISR
contains only timestamp/counter filtering.
