# Production firmware specification

The firmware is an autonomous bike computer: Hall speed acquisition, ST7796 touch UI, statistics, SD logging/recovery, battery telemetry, settings, diagnostics and USB MSC. It must boot without SD, Hall or FT6336.

Boot order: serial/build info; backlight/TFT splash; touch; SD; config (NVS defaults then valid SD config); Hall; battery; recovery; UI. No SD opens a Retry/Continue screen; Continue retains all live ride functions without persistent logging.

Speed uses accepted Hall intervals at microsecond resolution. One accepted pulse is `wheel_circumference_m / pulses_per_revolution`; pulses under configured debounce or exceeding maximum plausible speed are rejected. Raw, filtered and display speeds are separate. After the last interval expires, display speed is estimated from time since last pulse and naturally decays to zero below `stop_threshold_kmh` (default 3 km/h).

Ride state is exactly `IDLE`, `RIDING`, `PAUSED`, `FINISHED`. Distance comes only from accepted pulses while RIDING. `elapsed` covers Start→Finish, `recording` only RIDING, `moving` only RIDING while speed passes hysteresis, `pause` is elapsed−recording, and `stopped` is recording−moving. Main UI AVG is distance/moving time; it is never the arithmetic mean of samples. Finish requires confirmation, writes FINISH/summary and only then clears recovery.

The ride screen provides Speed, auto-scaled Graph and Stats pages by swipe. Main menu has Ride, History, Diagnostics, Settings, USB Storage and About. History reads only summary files; active rides cannot be deleted.

Battery GPIO6/ADC1 is production-enabled. A nonblocking sampling state machine discards initial readings, takes seven samples, applies trimmed mean, divider and stored calibration factor, then low-pass filters for UI. SoC uses a 1S Li-Po interpolation table with percentage hysteresis. States include normal/low/critical plus charging/discharging/stable trend; USB is not treated as proof of charging.

USB MSC has a strict owner model. RIDING is refused; PAUSED is checkpointed and files are closed; IDLE/PAUSED SD is exposed to host. No FAT operation is permitted while active. Safe eject followed by reboot is the supported exit path.

Diagnostics: display primitives, raw touch, paint, SD read/write/card information, USB, Hall raw state and counters, battery raw/calibrated values, and system info. Runtime work is timer/state-machine driven; ISR contains only timestamp/counter filtering.
