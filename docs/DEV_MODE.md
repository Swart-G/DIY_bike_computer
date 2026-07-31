# Dev Mode diagnostics and stability capture

Dev Mode is a separate on-device diagnostics screen in the normal firmware, not a
different build. Open **Settings → Diagnostics → Dev Mode**. While that screen is open,
the native ESP32-S3 USB Serial/JTAG endpoint emits one `DEV {json}` sample every two
seconds and accepts the bounded JSONL API described below. Leaving the screen disables
telemetry and commands. Dev Mode refuses to start while USB Mass Storage owns the SD
card.

Build and upload the normal firmware:

```bash
pio run
pio run -t upload --upload-port /dev/ttyACM0
```

After upload, open the serial connection first, then navigate to Dev Mode and capture
five minutes through `/dev/ttyACM*`. Opening or closing ESP32-S3 Serial/JTAG may reset
the board, so do not reconnect the host while a Dev session is running:

```bash
python3 scripts/capture_dev_monitor.py --port /dev/ttyACM0 --duration 300
```

The `--port` option may be omitted when exactly one supported WCH `1a86:55d3` or
Espressif `303a:1001` serial adapter is present. To watch the stream without saving it,
use:

```bash
pio device monitor --port /dev/ttyACM0 --baud 115200
```

Every received line is written as a host-timestamped JSONL record under
`artifacts/dev_monitor/`. Parsed device telemetry is stored in the same record. A
sidecar `.report.json` checks resets/panics, sequence and telemetry gaps, heap/PSRAM
minima and drift, main-loop latency and stack margin, touch/SD availability, logger
gaps and buffering, battery range/estimate quality, temperature, Hall counters and
phone-location accepted/rejected counters.

Each sample includes `location.available`, `fresh`, `ride_id`, UTC timestamp,
latitude/longitude, optional altitude/accuracy/speed, `age_ms`, and cumulative
`accepted`/`rejected`. During a real active ride, proof of end-to-end GPS delivery is
`accepted` increasing with `fresh:true` and matching non-null coordinates. The phone
capability bit alone is not proof. After five seconds without a new accepted packet,
`fresh` becomes false and subsequent ride CSV location columns remain blank.

Analyze a prior file without opening a port:

```bash
python3 scripts/capture_dev_monitor.py --analyze-only artifacts/dev_monitor/capture_YYYYMMDD_HHMMSS.jsonl
```

## Raw API

Commands are one JSON object per line on native USB Serial/JTAG. The optional `DEV ` prefix is
accepted. Responses and unsolicited samples are prefixed with `DEV `. Every command
should contain a numeric `id` and a `cmd`:

```bash
python3 scripts/dev_raw_api.py --command '{"id":1,"cmd":"help"}'
python3 scripts/dev_raw_api.py --command '{"id":2,"cmd":"snapshot"}'
python3 scripts/dev_raw_api.py --interactive
```

For an automated phone-location run after opening Dev Mode, the capture tool can start
and finish a real logged ride on the same serial connection (so reopening the port
cannot reset the board between steps):

```bash
python3 scripts/capture_dev_monitor.py --duration 60 --require-location \
  --dev-command '{"id":101,"cmd":"ride_control","action":"start"}' \
  --final-dev-command '{"id":102,"cmd":"ride_control","action":"finish"}'
```

Supported commands:

- `ping`, `help`, `snapshot`, `self_test`;
- `stream` with `enabled` and an `interval_ms` from 250 to 10000;
- `preview` with a source-screen index, or a negative index to clear;
- `sd_test`, refused while riding, logging, MSC, or when SD is unavailable;
- `rgb` with `r`, `g`, `b`, or `clear:true`;
- `media_action` with `play`, `pause`, `toggle`, `next`, `previous` or `seek`.
- `ride_control` with `start`, `pause`, `resume` or `finish`; this follows the
  normal logger/recovery path and is available only while the Dev Mode screen is open;
- `usb_storage` with `confirm:true`; after acknowledging it closes Dev USB and
  transfers the SD card to TinyUSB MSC. Safe eject followed by reboot remains the
  supported exit.

Input parsing and serial work are bounded on every main-loop pass. Leaving Dev Mode
clears screen/RGB overrides and disables the API.

Opening Dev Mode may make battery state read `Charging` when the USB data host is
present. This is intentional and does not force the stored percentage to 100%. Finish
the Dev capture and leave the screen before testing USB Mass Storage according to
`docs/TEST_PLAN.md`.
