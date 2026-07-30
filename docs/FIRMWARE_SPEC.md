# Production firmware specification

The firmware is an autonomous bike computer: Hall speed acquisition, ST7796 touch UI, statistics, SD logging/recovery, battery telemetry, settings, diagnostics and USB MSC. It must boot without SD, Hall or FT6336.

Boot order: serial/build info; backlight/TFT animated bicycle splash; touch; BLE startup
and a bounded animated power-settle interval; SD; config (NVS defaults then valid SD
config); Hall; battery; recovery; UI. Mounting SD after the radio's largest startup load
step avoids leaving an already-mounted card latched after a marginal 3.3 V transient.
The splash retains the
five latest bounded initialization results and advances the rotating wheel spokes and
road animation between stages. It is composed in the PSRAM framebuffer and only the
small animation region is transferred between stages, so erase operations are never
exposed as visible blinking.
No SD opens a Retry/Continue screen; Continue retains all live ride functions without
persistent logging.

SD mounting is fixed at 400 kHz because reliable shared-bus signalling is preferred to
throughput; there is no high-speed retry.
Before `SD.begin()` both chip selects remain high through a 20 ms quiet interval and
160 explicit 400 kHz idle clocks. A runtime open/write failure triggers up to three
bounded, progressively delayed remount attempts at 400 kHz without formatting. Before
unmount, firmware sends CRC-valid `CMD12` and waits for the card to leave busy state so
a stranded multi-block transfer cannot make the following `CMD0` ineffective. It then
fully stops and restarts the ESP32 SPI peripheral with the fixed hardware pins before
issuing idle clocks and mounting. Before SD
traffic, ST7796 is returned from register-read mode to RAM-write mode at 400 kHz. If the
display still holds shared MISO, later attempts keep ST7796 in hardware reset during
mount, then request immediate panel reinitialization and framebuffer restoration. The
SD diagnostic and ride start probe the mounted root through this recovery path before
their first write. Append is retried only when the file did not open, so a possible
partial CSV write is never duplicated. Only failed recovery changes the card to
unavailable and activates ride sample buffering.
Every bounded firmware SD session ends with both chip-select lines high and sixteen
idle SPI clocks before the display can reclaim the shared bus. This prevents the card
from carrying an unfinished SPI state into the next file open.
TFT traffic is capped at 10 MHz (4 MHz reads), and both CS outputs use maximum drive
strength with pull-ups to reduce false selection from shared-harness edge coupling.

The ST7796 is health-checked from the main loop using its readable control registers.
Two consecutive failed probes suspend panel writes while the latest PSRAM framebuffer
continues to update. Firmware retries panel initialization about every two seconds and
pushes that framebuffer after a successful probe, without rebooting the ESP. If register
reads were unavailable during boot, a slower discovery probe enables monitoring after
the panel appears. Display recovery is suspended while USB owns the shared SD bus.
FT6336 I2C communication is likewise retried once per second after repeated read errors.
The bus runs at a conservative 100 kHz, initialization observes the controller's 300 ms
post-reset reporting time, and a bounded 38 ms release grace masks a dropped scan without
delaying touch-down handling.

Speed uses accepted Hall intervals at microsecond resolution. One accepted pulse is `wheel_circumference_m / pulses_per_revolution`; pulses under configured debounce or exceeding maximum plausible speed are rejected. Raw, filtered and display speeds are separate. After the last interval expires, display speed is estimated from time since last pulse and naturally decays to zero below `stop_threshold_kmh` (default 3 km/h).

The built-in GPIO48 addressable RGB LED compares display speed with the sample from
two seconds earlier. A change above the configured stable tolerance is purple, a change
below the negative tolerance is red, and a change within the inclusive tolerance is
green. Before a full two-second history exists it is green. When enabled, Speed LED also
adds an F1-style Ride page with independent 2, 5 and 10 second segments. Each segment
shows its signed speed delta and uses its own persisted stable tolerance; the physical
LED exactly mirrors the 2-second segment. The Settings `Speed LED` section replaces
About and controls enable state, all three tolerances and LED brightness.
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
and auto-scaled Graph; the three-segment Speed Trend page is inserted when Speed LED is
enabled, while Navigation and Media are added only when their companion features are
available. RIDING uses Pause + Finish and PAUSED uses
Resume + Finish. Finish always confirms. History reads only summary files; active rides
cannot be deleted.

Home derives both its phone icon and `Phone connected`/`Phone not connected` subtitle
from the same live `PhoneLinkManager::ready()` value. Firmware stores up to four
application associations in a bounded NVS registry. The Phone screen lists their
persisted display names, marks the active entry and exposes Add phone plus an explicit
Forget all action. A legacy single association is migrated into the first registry slot.

Cancel on the pairing screen closes the current GATT link and runtime pairing window but
preserves previously remembered phones and their controller bonds. Only `Forget all`
clears the application registry and every stored BLE bond. Normal non-pairable
advertising remains available for bonded reconnects. Hall, ride state and logging
continue unchanged.

The first authorized phone session may immediately request the SD ride manifest. Its
bounded protocol and 24-item repository scratch buffers are persistent members rather
than `loopTask` locals, leaving stack headroom for FatFs and JSON parsing. A newly
authenticated passkey session creates a new registry entry; an ordinary bonded reconnect
must still present one of the exact stored identifiers.

Runtime passkey configuration must leave BLE authentication in Secure Connections +
MITM + Bonding mode. The ESP32 Arduino `setStaticPIN()` helper changes the authentication
mode, so firmware reapplies the bonding/key-distribution parameters after every PIN
change. A normal disconnect, app restart or ESP reboot reuses the stored LTK without
showing another PIN; only explicit Forget all removes remembered bonds.
At boot, a non-empty application registry without any controller bond is unusable and is
cleared automatically so the Phone screen returns to unpaired instead of entering a
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
The common header prints SoC beside the battery and a `~ Nh NN m` runtime estimate.
Runtime is learned from the observed percentage decline over at least five minutes,
smoothed between updates and withheld as `~ --` until sufficient discharge history
exists; charging is shown as `CHG` instead of a fabricated remaining time.

USB MSC has a strict owner model. RIDING is refused; PAUSED is checkpointed and files are closed; IDLE/PAUSED SD is exposed to host. No FAT operation is permitted while active. Safe eject followed by reboot is the supported exit path.

Diagnostics: display primitives, both raw FT6336 touch points, paint, SD read/write/card
information, USB, Hall raw state and counters, battery raw/calibrated values, and system
info including BLE/protocol state. Runtime work is timer/state-machine driven; ISR
contains only timestamp/counter filtering.

Every header Back chevron has a 126×58 touch target even though its visual remains
compact. Touch Raw maps both contacts into a dedicated bordered 444×98 field that never
overlaps its Paint or Back controls.
