# Manual production test plan

For every section record firmware version, SD brand and battery voltage. Failure symptoms are a reason to stop and retain the Serial log; never format a card containing evidence.

## 1. Build and boot

Steps: run `pio run`, upload, open `pio device monitor`, boot with all peripherals attached. Expected: version 1.0.0, memory information and one result per init stage; splash then Menu/Recovery. Failure: compile error, boot loop, black display or repeated error spam.

## 2. Boot without optional hardware

Steps: remove SD, then separately disconnect Hall and FT6336; reboot and press Continue on no-SD screen. Expected: no reset; Ride, Settings and diagnostics still work; status says NO SD / NO TOUCH where relevant. Failure: reset loop, frozen touch or attempt to write SD.

## 3. Display and touch

Steps: Settings → Diagnostics → Display; inspect RGB/white/black, primitives/text/orientation. Diagnostics → Touch raw; tap all four corners and test two simultaneous points. Diagnostics → Paint; draw rapid diagonals, Clear, Back. Expected: 480×320 landscape, fixed normal backlight, both FT6336 points map correctly, continuous paint line. Failure: flicker, missing colours, shifted/inverted touch or UI overlap.

## 4. Hall and speed

Steps: Diagnostics → Hall; observe unconnected level. Feed safe 3.3 V/open-drain pulses on GPIO4 at known intervals, then pulses below `min_pulse_interval_ms`. Stop pulses. Expected: accepted/rejected counters, last interval, raw/filtered speed; rejected noise; speed holds for one interval then naturally falls to zero below 3 km/h. Failure: counting without pulses, missed valid pulses, fixed timeout jump, or >3.3 V applied.

## 5. Battery

Steps: power from a measured 1S pack; Diagnostics → Battery; wait for several sample series; compare filtered voltage with meter; use CAL −/+ then SAVE; reboot. Expected: GPIO6 raw ADC, ADC mV, instant/filtered battery voltage, factor, SoC and trend; saved calibration survives reboot. Failure: static value, implausible voltage, fast percentage jitter or blocking UI. Verify low/critical visual state with a controlled safe supply; do not over-discharge a Li-Po.

## 6. SD

Steps: boot with FAT SD; Diagnostics → SD → Run test; inspect card type/capacity/free space/frequency and `/BIKE_SPEEDOMETER_SD_TEST.txt` on a computer. Repeat after reinserting card. Expected: read/write match, no format prompt, fallback frequency if required. Failure: UI crash, automatic format, corrupt test text or continuous retries.

## 7. Ride lifecycle and statistics

Steps: Start, generate pulses while moving, stop without pausing, Pause, generate more pulses, Resume, then Finish and confirm. Expected: distance only grows on accepted RIDING pulses; elapsed includes pause; recording excludes pause; moving excludes standing; stopped=recording−moving; AVG=distance/moving; paused pulses are ignored. Finish shows summary and creates a new-ride/menu choice. Failure: instant finish without confirmation, reset stats before summary, or paused movement counted.

## 8. Ride files and history

Steps: inspect `/rides/ride_XXXXXX/` after Finish. Expected: `meta.json`, append-only `samples.csv`, `events.csv`, `summary.json`; headers exactly match LOG_FORMAT; no fake date/GPS. Menu → History shows summary-only rows and details and vertically scrolls when more than three rides exist. Verify Delete requires explicit confirmation, Cancel preserves the ride, confirmed deletion removes the complete ride folder even when it contains an additional file, and USB Storage enters MSC from the detail screen. Attempt deleting finished ride and active ride. Failure: malformed CSV/JSON, missing FINISH summary, deletion without confirmation, a retained/dead History row, wrong ride opened after scrolling, active ride deletion, or history requiring samples scan.

## 9. Recovery

Steps: Start a ride, wait one recovery interval, remove power during RIDING; reboot. Repeat from PAUSED and repeat with SD removed after an NVS checkpoint. Expected: Recovery screen always offers Resume/Finish/Discard and state is PAUSED; never auto-RIDING; Resume appends existing CSV and event; Finish makes summary then removes recovery. Failure: lost stats, a new folder for resumed ride, or auto-start.

## 10. SD removal during ride

Steps: start and generate pulses, remove SD, keep riding 2–3 minutes, reinsert, wait for retry, finish. Expected: speed/stats/UI continue; clear SD ERROR; samples are buffered then `SD_RESTORED` is appended when card returns; summary exposes logging gap if data could not fit. Failure: reset, blocked Hall interrupt, a false claim that all data was saved.

## 11. USB Mass Storage

Steps: from IDLE start USB; from PAUSED checkpoint then start USB; from RIDING request USB. Connect host, inspect/edit a copied file, safe eject, then reboot device. Expected: host sees SD; active screen says USB Storage Active; firmware does no FAT actions; RIDING is refused with guidance; reboot restores normal SD ownership. Failure: host corruption, concurrent logging/history, or USB exit without reinitialisation.

## 12. Settings persistence

Steps: change circumference, stop threshold and battery factor; Save; reboot with SD, then without SD. Also boot with a v1 config containing `display_brightness_percent`. Expected: ranges clamp invalid values, settings apply immediately, valid SD config supersedes NVS, NVS preserves essential values without SD, and the legacy brightness field is safely ignored. Failure: GPIO editing exposed, invalid values crash boot, or calibration does not persist.

## 13. Long-duration test

Steps: run at least three hours with real or pulse-generator input; vary speed, pauses and graph pages; retain SD logs; note free/min heap at start/end. Expected: no watchdog reset, stable counters, monotonic samples, no unbounded heap decline, rolling graph and files grow normally across `millis()`-like long operation. Failure: missed pulses under UI load, fragmentation, SD corruption, stuck graph or bad timers.

## 14. Speed trend RGB LED

Steps: open Settings → Speed LED; verify enable/disable, cycle stable range and
brightness, then reboot with and without SD. With a pulse generator, hold speed steady
for more than two seconds, increase it beyond the configured tolerance, hold again, and
decrease it beyond the tolerance. Expected: GPIO48 LED is green within the inclusive
range, purple when the two-second speed delta is positive, red when negative, and off
when disabled. Settings survive both reboot cases; Hall counts and UI remain responsive.
Failure: wrong colour order, flicker, a blocking loop, changes inside the tolerance, or
LED traffic from the Hall ISR.
