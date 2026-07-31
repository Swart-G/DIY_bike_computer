#!/usr/bin/env python3
"""Send bounded JSON commands to the Bike Computer Diagnostics Dev Mode API."""

from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any

import serial

from capture_dev_monitor import DEV_PREFIX, auto_port


def open_port(port: str, baud: int) -> serial.Serial:
    connection = serial.Serial(port=None, baudrate=baud, timeout=0.2)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    return connection


def normalize_command(raw: str, request_id: int) -> dict[str, Any]:
    value = json.loads(raw)
    if not isinstance(value, dict):
        raise ValueError("command must be a JSON object")
    value.setdefault("id", request_id)
    if not isinstance(value.get("id"), int) or not value.get("cmd"):
        raise ValueError("command requires an integer id and a cmd string")
    return value


def transact(connection: serial.Serial, command: dict[str, Any], timeout: float) -> bool:
    payload = json.dumps(command, ensure_ascii=False, separators=(",", ":"))
    connection.write(payload.encode("utf-8") + b"\n")
    connection.flush()
    deadline = time.monotonic() + timeout
    expected_id = command["id"]
    while time.monotonic() < deadline:
        raw = connection.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
        print(text)
        if not text.startswith(DEV_PREFIX):
            continue
        try:
            response = json.loads(text[len(DEV_PREFIX) :])
        except json.JSONDecodeError:
            continue
        if response.get("id") == expected_id and response.get("type") in {
            "response",
            "snapshot",
        }:
            return bool(response.get("ok", True))
    print(f"timeout waiting for response id={expected_id}", file=sys.stderr)
    return False


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port; auto-detected when omitted")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument(
        "--command",
        default='{"cmd":"help"}',
        help="single JSON command; defaults to help",
    )
    parser.add_argument("--interactive", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    port = args.port or auto_port()
    with open_port(port, args.baud) as connection:
        if not args.interactive:
            command = normalize_command(args.command, 1)
            return 0 if transact(connection, command, args.timeout) else 1

        request_id = 1
        print(f"Connected to {port}. Enter JSON commands; Ctrl-D exits.")
        while True:
            try:
                raw = input("dev> ").strip()
            except EOFError:
                print()
                return 0
            if not raw:
                continue
            try:
                command = normalize_command(raw, request_id)
                transact(connection, command, args.timeout)
                request_id += 1
            except (ValueError, json.JSONDecodeError) as exc:
                print(f"invalid command: {exc}", file=sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
