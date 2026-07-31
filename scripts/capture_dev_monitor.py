#!/usr/bin/env python3
"""Capture and analyze Bike Computer Dev-mode JSONL telemetry."""

from __future__ import annotations

import argparse
import json
import math
import statistics
import sys
import time
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

import serial
from serial.tools import list_ports


ESPRESSIF_VID = 0x303A
USB_JTAG_PID = 0x1001
WCH_VID = 0x1A86
WCH_SINGLE_SERIAL_PID = 0x55D3
DEV_PREFIX = "DEV "
FATAL_MARKERS = (
    "guru meditation",
    "abort() was called",
    "brownout detector",
    "task watchdog",
    "stack canary",
    "assert failed",
    "panic'ed",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def auto_port() -> str:
    ports = list(list_ports.comports())
    preferred = [
        port
        for port in ports
        if port.vid == ESPRESSIF_VID and port.pid == USB_JTAG_PID
    ]
    if len(preferred) == 1:
        return preferred[0].device
    if len(preferred) > 1:
        names = ", ".join(port.device for port in preferred)
        raise RuntimeError(f"multiple ESP32-S3 ports found ({names}); use --port")
    uart = [
        port
        for port in ports
        if port.vid == WCH_VID and port.pid == WCH_SINGLE_SERIAL_PID
    ]
    if len(uart) == 1:
        return uart[0].device
    if len(uart) > 1:
        names = ", ".join(port.device for port in uart)
        raise RuntimeError(f"multiple bike-computer UART ports found ({names}); use --port")
    if len(ports) == 1:
        return ports[0].device
    names = ", ".join(port.device for port in ports) or "none"
    raise RuntimeError(f"bike-computer serial port not found; available: {names}")


def default_output() -> Path:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return Path("artifacts/dev_monitor") / f"capture_{stamp}.jsonl"


def nested_values(samples: Iterable[dict[str, Any]], *path: str) -> list[Any]:
    values: list[Any] = []
    for sample in samples:
        value: Any = sample
        for key in path:
            if not isinstance(value, dict) or key not in value:
                value = None
                break
            value = value[key]
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            if isinstance(value, float) and not math.isfinite(value):
                continue
            values.append(value)
    return values


def numeric_summary(values: list[Any]) -> dict[str, float | int | None]:
    if not values:
        return {"first": None, "last": None, "min": None, "max": None, "mean": None}
    return {
        "first": values[0],
        "last": values[-1],
        "min": min(values),
        "max": max(values),
        "mean": statistics.fmean(values),
    }


def load_capture(path: Path) -> tuple[list[dict[str, Any]], list[str], int]:
    records: list[dict[str, Any]] = []
    raw_lines: list[str] = []
    invalid_records = 0
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                invalid_records += 1
                continue
            if isinstance(record, dict):
                records.append(record)
                raw_lines.append(str(record.get("raw", "")))
    return records, raw_lines, invalid_records


def analyze_capture(path: Path, require_location: bool = False) -> dict[str, Any]:
    records, raw_lines, invalid_records = load_capture(path)
    dev_records = [record["dev"] for record in records if isinstance(record.get("dev"), dict)]
    samples = [item for item in dev_records if item.get("type") == "sample"]
    boots = [item for item in dev_records if item.get("type") == "boot"]
    device_errors = [item for item in dev_records if item.get("type") == "error"]
    invalid_dev_json = sum(
        1 for record in records if str(record.get("raw", "")).startswith(DEV_PREFIX) and "dev" not in record
    )

    uptimes = nested_values(samples, "uptime_ms")
    sequences = nested_values(samples, "seq")
    telemetry_gaps = [b - a for a, b in zip(uptimes, uptimes[1:]) if b >= a]
    uptime_rollbacks = sum(1 for a, b in zip(uptimes, uptimes[1:]) if b < a)
    sequence_gaps = sum(max(0, int(b - a - 1)) for a, b in zip(sequences, sequences[1:]) if b >= a)

    fatal_lines = [
        line for line in raw_lines if any(marker in line.lower() for marker in FATAL_MARKERS)
    ]
    heap_free = nested_values(samples, "memory", "heap_free")
    heap_min = nested_values(samples, "memory", "heap_min")
    psram_free = nested_values(samples, "memory", "psram_free")
    loop_hz = nested_values(samples, "loop", "hz")
    loop_max_us = nested_values(samples, "loop", "max_period_us")
    stack_water = nested_values(samples, "loop", "stack_high_water_words")
    temperature = nested_values(samples, "system", "temperature_c")
    battery_v = nested_values(samples, "battery", "filtered_v")
    battery_percent = nested_values(samples, "battery", "percent")
    hall_pulses = nested_values(samples, "hall", "pulses")
    hall_rejected = nested_values(samples, "hall", "rejected")
    buffered = nested_values(samples, "storage", "buffered_samples")
    runtime_quality = nested_values(samples, "battery", "runtime_quality")
    location_accepted = nested_values(samples, "location", "accepted")
    location_rejected = nested_values(samples, "location", "rejected")
    location_fresh_samples = sum(
        sample.get("location", {}).get("fresh") is True for sample in samples
    )

    warnings: list[str] = []
    failures: list[str] = []
    if not samples:
        failures.append("no Dev telemetry samples captured")
    if fatal_lines:
        failures.append(f"{len(fatal_lines)} fatal/panic marker(s) found")
    if uptime_rollbacks or len(boots) > 1:
        failures.append("unexpected reboot detected")
    if invalid_records or invalid_dev_json or device_errors:
        failures.append("capture contains invalid or device-error telemetry records")
    if sequence_gaps:
        warnings.append(f"{sequence_gaps} telemetry sequence value(s) missing")
    if telemetry_gaps and max(telemetry_gaps) > 5000:
        warnings.append(f"maximum telemetry gap is {max(telemetry_gaps)} ms")
    if heap_free and min(heap_free) < 20000:
        failures.append(f"free heap fell below 20 KiB ({min(heap_free)} bytes)")
    if len(heap_free) >= 2 and heap_free[-1] < heap_free[0] - max(8192, heap_free[0] * 0.10):
        warnings.append(f"free heap drifted by {heap_free[-1] - heap_free[0]} bytes")
    if loop_max_us and max(loop_max_us) > 500000:
        warnings.append(f"main-loop period exceeded 500 ms ({max(loop_max_us)} us)")
    if stack_water and min(stack_water) < 512:
        warnings.append(f"loop task stack high-water mark is low ({min(stack_water)} words)")
    if any(not sample.get("touch", {}).get("ready", False) for sample in samples):
        warnings.append("touch controller was unavailable in at least one sample")
    if any(sample.get("storage", {}).get("logging_gap", False) for sample in samples):
        warnings.append("ride logger reported a logging gap")
    if any(not sample.get("storage", {}).get("sd_available", False) for sample in samples):
        warnings.append("SD was unavailable in at least one sample (allowed no-SD mode)")
    location_rejected_delta = (
        location_rejected[-1] - location_rejected[0]
        if len(location_rejected) >= 2 else 0
    )
    if location_rejected_delta:
        warnings.append(
            f"firmware rejected {location_rejected_delta} phone location packet(s)"
        )
    location_accepted_delta = (
        location_accepted[-1] - location_accepted[0]
        if len(location_accepted) >= 2 else 0
    )
    if require_location and location_accepted_delta <= 0:
        failures.append("no phone location packet was accepted")
    if require_location and location_fresh_samples <= 0:
        failures.append("no fresh phone location fix appeared in Dev telemetry")

    quality_counts = Counter(int(value) for value in runtime_quality)
    report = {
        "capture": str(path),
        "generated_utc": utc_now(),
        "status": "FAIL" if failures else ("WARN" if warnings else "PASS"),
        "failures": failures,
        "warnings": warnings,
        "records": len(records),
        "dev_records": len(dev_records),
        "samples": len(samples),
        "boots": len(boots),
        "invalid_capture_records": invalid_records,
        "invalid_dev_json": invalid_dev_json,
        "device_error_records": len(device_errors),
        "uptime_duration_ms": (uptimes[-1] - uptimes[0]) if len(uptimes) >= 2 else 0,
        "uptime_rollbacks": uptime_rollbacks,
        "sequence_gaps": sequence_gaps,
        "telemetry_gap_ms": numeric_summary(telemetry_gaps),
        "heap_free": numeric_summary(heap_free),
        "heap_min": numeric_summary(heap_min),
        "psram_free": numeric_summary(psram_free),
        "loop_hz": numeric_summary(loop_hz),
        "loop_max_period_us": numeric_summary(loop_max_us),
        "stack_high_water_words": numeric_summary(stack_water),
        "temperature_c": numeric_summary(temperature),
        "battery_voltage": numeric_summary(battery_v),
        "battery_percent": numeric_summary(battery_percent),
        "runtime_quality_counts": dict(sorted(quality_counts.items())),
        "hall_pulse_delta": (hall_pulses[-1] - hall_pulses[0]) if len(hall_pulses) >= 2 else 0,
        "hall_rejected_delta": (hall_rejected[-1] - hall_rejected[0]) if len(hall_rejected) >= 2 else 0,
        "maximum_buffered_samples": max(buffered) if buffered else None,
        "location_required": require_location,
        "location_accepted_delta": location_accepted_delta,
        "location_rejected_delta": location_rejected_delta,
        "location_fresh_samples": location_fresh_samples,
        "fatal_lines": fatal_lines[:20],
    }
    return report


def print_report(report: dict[str, Any]) -> None:
    print(f"Status: {report['status']}")
    print(
        f"Samples: {report['samples']}, duration: {report['uptime_duration_ms'] / 1000:.1f} s, "
        f"sequence gaps: {report['sequence_gaps']}"
    )
    heap = report["heap_free"]
    loop = report["loop_max_period_us"]
    print(
        f"Heap: first={heap['first']} last={heap['last']} min={heap['min']} bytes; "
        f"max loop period={loop['max']} us"
    )
    print(
        f"Location: accepted delta={report['location_accepted_delta']}, "
        f"rejected delta={report['location_rejected_delta']}, "
        f"fresh samples={report['location_fresh_samples']}"
    )
    for failure in report["failures"]:
        print(f"FAIL: {failure}")
    for warning in report["warnings"]:
        print(f"WARN: {warning}")


def encode_commands(raw_commands: list[str]) -> list[bytes]:
    encoded: list[bytes] = []
    for raw in raw_commands:
        command = json.loads(raw)
        if not isinstance(command, dict) or not isinstance(command.get("id"), int):
            raise ValueError("each Dev command requires a JSON object with an integer id")
        if not isinstance(command.get("cmd"), str) or not command["cmd"]:
            raise ValueError("each Dev command requires a non-empty cmd string")
        encoded.append(
            (json.dumps(command, ensure_ascii=False, separators=(",", ":")) + "\n").encode()
        )
    return encoded


def capture(
    port: str,
    baud: int,
    duration: float,
    output: Path,
    initial_commands: list[bytes],
    final_commands: list[bytes],
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    print(f"Capturing {port} at {baud} baud to {output} for {duration:.0f} s")
    connection = serial.Serial(port=None, baudrate=baud, timeout=0.25)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    with connection, output.open("w", encoding="utf-8", buffering=1) as handle:
        connection.dtr = False
        connection.rts = False
        dev_seen = False
        final_sent_at: float | None = None
        while True:
            elapsed = time.monotonic() - started
            if duration > 0 and elapsed >= duration and final_sent_at is None:
                if dev_seen and final_commands:
                    for command in final_commands:
                        connection.write(command)
                    connection.flush()
                    final_sent_at = time.monotonic()
                else:
                    break
            if final_sent_at is not None and time.monotonic() - final_sent_at >= 2.0:
                break
            raw = connection.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            record: dict[str, Any] = {
                "host_time_utc": utc_now(),
                "host_elapsed_s": round(time.monotonic() - started, 6),
                "raw": text,
            }
            if text.startswith(DEV_PREFIX):
                try:
                    record["dev"] = json.loads(text[len(DEV_PREFIX) :])
                except json.JSONDecodeError as exc:
                    record["dev_parse_error"] = str(exc)
            handle.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n")
            print(text)
            if text.startswith(DEV_PREFIX) and not dev_seen:
                dev_seen = True
                for command in initial_commands:
                    connection.write(command)
                connection.flush()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port; auto-detected when omitted")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=300.0, help="seconds; 0 means until Ctrl-C")
    parser.add_argument("--output", type=Path, default=default_output())
    parser.add_argument("--analyze-only", type=Path, metavar="CAPTURE")
    parser.add_argument(
        "--require-location",
        action="store_true",
        help="fail analysis unless at least one fresh phone fix was accepted",
    )
    parser.add_argument(
        "--dev-command",
        action="append",
        default=[],
        metavar="JSON",
        help="send after the first Dev record; repeatable",
    )
    parser.add_argument(
        "--final-dev-command",
        action="append",
        default=[],
        metavar="JSON",
        help="send at capture end, then drain responses for two seconds; repeatable",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    capture_path = args.analyze_only or args.output
    capture_error: str | None = None
    if not args.analyze_only:
        try:
            capture(
                args.port or auto_port(),
                args.baud,
                args.duration,
                capture_path,
                encode_commands(args.dev_command),
                encode_commands(args.final_dev_command),
            )
        except KeyboardInterrupt:
            print("Capture stopped by user")
        except (OSError, RuntimeError, ValueError, json.JSONDecodeError,
                serial.SerialException) as exc:
            print(f"Capture failed: {exc}", file=sys.stderr)
            capture_error = str(exc)
            if not capture_path.exists():
                return 2
    report = analyze_capture(capture_path, require_location=args.require_location)
    if capture_error:
        report["failures"].append(f"serial capture ended with error: {capture_error}")
        report["status"] = "FAIL"
    report_path = capture_path.with_suffix(".report.json")
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print_report(report)
    print(f"Report: {report_path}")
    return 1 if report["status"] == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
