# Manual production test plan

For every section record firmware version, SD brand and battery voltage. Failure symptoms are a reason to stop and retain the Serial log; never format a card containing evidence.

## 1. Build and boot

Steps: run `pio run`, upload, open `pio device monitor`, boot with all peripherals
attached. Expected: version `2.1.0-dev`, memory information and one result per init
stage; the bicycle wheel spokes and road visibly advance without a blank-frame flash,
the latest boot-stage logs remain readable, then Menu/Recovery opens. Failure: compile
error, boot loop, static/black display, dots on the wheel rims, visible splash blinking
or repeated error spam.

## 2. Boot without optional hardware

Steps: remove SD, then separately disconnect Hall and FT6336; reboot and press Continue on no-SD screen. Expected: no reset; Ride, Settings and diagnostics still work; status says NO SD / NO TOUCH where relevant. Failure: reset loop, frozen touch or attempt to write SD.

## 3. Display and touch

Steps: Settings → Diagnostics → Display; inspect RGB/white/black,
primitives/text/orientation. While on a normal screen disconnect the TFT, wait for the
Serial health failure, reconnect it without resetting power and wait up to five seconds.
Diagnostics → Touch raw; tap all four corners and test two simultaneous points. Briefly
disconnect/reconnect FT6336. Diagnostics → Paint; draw rapid diagonals, Clear, Back.
Make repeated short taps and slow presses across Home and Settings.
Expected: 480×320 landscape, fixed normal backlight, the last UI frame returns
automatically after TFT reconnection, FT6336 returns within about one second, both touch
points stay inside the bordered field without covering Paint/Back, every touch-down
registers once without a false release while held, and paint is continuous. Failure:
reboot, permanent black display, flicker, missing colours, missed taps, duplicated taps,
shifted/inverted touch or UI overlap.

## 4. Hall and speed

Steps: Diagnostics → Hall; observe unconnected level. Feed safe 3.3 V/open-drain pulses on GPIO4 at known intervals, then pulses below `min_pulse_interval_ms`. Stop pulses. Expected: accepted/rejected counters, last interval, raw/filtered speed; rejected noise; speed holds for one interval then naturally falls to zero below 3 km/h. Failure: counting without pulses, missed valid pulses, fixed timeout jump, or >3.3 V applied.

## 5. Battery

Steps: power from a measured 1S pack; Diagnostics → Battery; wait for several sample
series; compare filtered voltage with meter; use CAL −/+ then SAVE; reboot. Continue a
controlled discharge for at least five minutes and long enough to observe a one-percent
drop, then connect a charger. Expected: GPIO6 raw ADC, ADC mV, instant/filtered battery
voltage, factor, SoC and trend; every header shows the percent and initially `~ --`, then
a smoothed `~ Nh NN m` estimate; charging shows `CHG`; saved calibration survives
reboot. Failure: fabricated time before a usable decline, static value, implausible
voltage, fast percentage/time jitter or blocking UI. Verify low/critical visual state
with a controlled safe supply; do not over-discharge a Li-Po.

## 6. SD

Steps: boot with FAT SD; Diagnostics → SD → Run test; inspect card
type/capacity/free space/frequency and `/SDTEST.TXT` on a computer. The test separately
verifies short-name root write/read and creation/validation of `/config`.
Repeat after reinserting the card and immediately after a cold boot. Provoke one bounded
open failure if possible and retain the Serial log. Expected: read/write match, no
format prompt, three consecutive internal write/read cycles pass on the first Run,
`SPI kHz` is 400, and `I/O recoveries` remains zero on healthy wiring. A
transient failure performs at most three progressively delayed 400 kHz remount attempts
and reports `SD recovered at 400 kHz, attempt N`; it never formats the card. Immediately
start a ride and confirm the root preflight, `/rides` and the unique ride directory,
`meta.json`, both CSV headers and START all complete without ENODEV.
Failure: UI crash, automatic format, corrupt test text, repeated append data, ENODEV on
cycle 1/2/3 or ride start, reuse of an existing ride directory, or continuous retries.
Also retain a cold-boot Serial trace: BLE must initialize before SD mount, and the first
post-boot SD access must not show `Card Failed! cmd: 0x0d`. If a display-isolated retry
is forced, confirm the display is restored from its framebuffer without an ESP restart
and the card becomes usable without removing device power.
If `CMD13` still disappears at 400 kHz, record socket 3.3 V with a scope or min/max
meter during BLE startup and full-screen transfer, then repeat with the specified local
decoupling, external CS pull-up and a known-good FAT32 card. Failure after those checks
is a card/socket/wiring fault, not a filesystem-format condition.

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

Steps: verify Auto Pause uses an inline toggle and its delay opens a dedicated editor
when tapping outside the switch. Open the dedicated editors for wheel circumference,
stop threshold and log interval. Change each value and press Back;
reopen and verify the old value remains. Change again and press Save. Change battery
factor; reboot with SD, then without SD. Also boot with a v1 config containing
`display_brightness_percent`. Expected: numeric summary rows never mutate values
directly; Back discards drafts; Save validates and applies; ranges clamp invalid values;
valid SD config supersedes NVS; NVS preserves essential values without SD; and the
legacy brightness field is safely ignored. Read-only Display/System rows have no
chevron. Failure: GPIO editing exposed, direct numeric changes from a summary row,
Back applying a draft, invalid values crashing boot, or calibration not persisting.

## 13. Long-duration test

Steps: run at least three hours with real or pulse-generator input; vary speed, pauses and graph pages; retain SD logs; note free/min heap at start/end. Expected: no watchdog reset, stable counters, monotonic samples, no unbounded heap decline, rolling graph and files grow normally across `millis()`-like long operation. Failure: missed pulses under UI load, fragmentation, SD corruption, stuck graph or bad timers.

## 14. Speed trend RGB LED

Steps: open Settings → Speed LED; verify Indicator uses an inline toggle. Open the
dedicated 2 s, 5 s, 10 s Stable range and Brightness editors, test Back cancellation and
Save, then reboot with and without SD. With a pulse generator, hold speed steady for
more than ten seconds, increase it beyond the configured tolerances, hold again, and
decrease it beyond the tolerances. Swipe through Ride pages. Expected: the additional
F1-style page exists only while Indicator is enabled; all three segments independently
show 2/5/10-second signed deltas. GPIO48 LED has exactly the 2-second segment colour:
green within the inclusive range, purple when positive, red when negative, and off when
disabled. Settings survive both reboot cases; Hall counts and UI remain responsive.
Failure: wrong colour order, flicker, a blocking loop, changes inside the tolerance, or
LED traffic from the Hall ISR.

## 15. Remembered phones

Steps: pair phone A and complete HELLO, disconnect it, return to Phone, then use Add
phone to pair phone B. Reboot and reconnect each phone in turn. Cancel a third pairing
window, then use Forget all. Expected: A and B remain as separate named rows after
disconnect and reboot, only the active row is green, Cancel preserves A/B, no more than
four rows can be added, and Forget all clears both the list and BLE bonds. Failure:
pairing B silently replaces A, Cancel deletes existing phones, an unknown association is
authorized, or the list disappears after reboot.

## 16. Common navigation targets

Steps: on every Diagnostics, History, Phone and Settings detail screen tap around the
visible upper-left chevron, including 20–30 px below/right of the glyph. Expected: the
126×58 target returns exactly one level and never activates a neighbouring control.
Failure: missed Back taps, double navigation or accidental value changes.
