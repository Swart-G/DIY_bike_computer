# Bike Protocol over BLE

Protocol status: version `1`, little-endian. Firmware, protocol and ride format versions
are independent.

## GATT

Service and characteristic UUIDs:

| Item | UUID | Properties |
|---|---|---|
| Bike Computer service | `f5c10000-7d2a-4b8e-9c31-5a7e2d9b0001` | primary service |
| Device Info | `f5c10001-7d2a-4b8e-9c31-5a7e2d9b0001` | read |
| RX | `f5c10002-7d2a-4b8e-9c31-5a7e2d9b0001` | write, write without response |
| TX | `f5c10003-7d2a-4b8e-9c31-5a7e2d9b0001` | notify, indicate |

Device Info is a compact, non-secret advertising/read structure:

```text
u8 protocol_min
u8 protocol_max
u8 firmware_major
u8 firmware_minor
u8 firmware_patch
u8 firmware_stage       0=release, 1=dev
u32 capability_flags
u64 device_id
```

Protocol 1 uses queued TX notifications for all traffic. The characteristic also
advertises `indicate` for forward compatibility, but the current ESP32 Arduino
`indicate()` path can block for about a second; protocol ACK/sequence rules provide
reliability without blocking the Hall/ride loop. Implementations must honor the
negotiated ATT MTU.

## Frame

```text
offset size field
0      2    magic = 0x4342       (wire bytes 42 43, ASCII "BC")
2      1    protocol_version
3      1    message_type
4      1    flags
5      2    sequence
7      2    payload_length
9      N    payload
9+N    2    crc16
```

All multibyte integers and IEEE-754 floats are little-endian. CRC is
CRC-16/CCITT-FALSE over bytes `0..8+N`: polynomial `0x1021`, initial `0xFFFF`,
no reflection, xor-out `0x0000`. Frame overhead is 11 bytes.

`payload_length` is limited by implementation memory. Each GATT notification/write is
no larger than `ATT_MTU - 3`; transport may split one protocol frame across several
ordered GATT packets, and the receiver feeds them into the stream decoder. File chunk
data is selected dynamically so its complete protocol frame fits one negotiated
packet. The stream decoder must handle partial frames, multiple frames, invalid magic,
unsupported version, oversized payload and CRC mismatch without allocation in the hot
path.

Flags:

```text
0x01 ACK_REQUIRED
0x02 RESPONSE
0x04 ERROR
0x08 MORE
0x10 PRIVILEGED
```

Sequence is a per-direction wrapping `u16`. Responses repeat the request sequence.

## Message IDs

| ID | Name | Direction |
|---:|---|---|
| `0x01` | HELLO | Android -> ESP |
| `0x02` | HELLO_ACK | ESP -> Android |
| `0x03` | ERROR | both |
| `0x04` | PING | both |
| `0x05` | PONG | both |
| `0x10` | TIME_SYNC | Android -> ESP |
| `0x20` | LIVE_TELEMETRY | ESP -> Android |
| `0x21` | RIDE_EVENT | ESP -> Android |
| `0x22` | DEVICE_EVENT | ESP -> Android |
| `0x30` | RIDE_LIST_REQUEST | Android -> ESP |
| `0x31` | RIDE_MANIFEST | ESP -> Android |
| `0x32` | RIDE_LIST_END | ESP -> Android |
| `0x33` | RIDE_DOWNLOAD_REQUEST | Android -> ESP |
| `0x34` | FILE_BEGIN | ESP -> Android |
| `0x35` | FILE_CHUNK | ESP -> Android |
| `0x36` | FILE_ACK | Android -> ESP |
| `0x37` | FILE_END | ESP -> Android |
| `0x38` | TRANSFER_CANCEL | both |
| `0x40` | MEDIA_STATE | Android -> ESP |
| `0x41` | MEDIA_ACTION | ESP -> Android |
| `0x50` | NAV_STATE | Android -> ESP |
| `0x60` | CONFIG_GET | Android -> ESP |
| `0x61` | CONFIG_VALUE | ESP -> Android |
| `0x62` | CONFIG_SET | Android -> ESP |
| `0x63` | CONFIG_RESULT | ESP -> Android |

Unknown IDs return `UNSUPPORTED_MESSAGE` when an ACK was requested and are otherwise
ignored.

## Capabilities

```text
bit 0 LIVE_TELEMETRY
bit 1 RIDE_SYNC
bit 2 MEDIA_CONTROL
bit 3 GPS_ASSIST
bit 4 NAVIGATION
bit 5 CONFIG_SYNC
bit 16 OTA             reserved, not implemented
bit 17 HEART_RATE      reserved, not implemented
bit 18 CADENCE         reserved, not implemented
bit 19 POWER           reserved, not implemented
```

## HELLO

Android sends:

```text
u8 app_major, app_minor, app_patch
u8 protocol_count
u8 protocol_versions[protocol_count]
u64 association_id       0 before association
u32 requested_capabilities
u8 name_len
u8 app_instance_name[name_len]  UTF-8
```

ESP selects one mutually supported version and replies:

```text
u8 selected_protocol
u8 firmware_major, firmware_minor, firmware_patch
u8 firmware_stage
u64 device_id
u64 association_id
u32 capability_flags
u8 ride_format_count
u8 ride_format_versions[ride_format_count]
u8 display_name_len
u8 display_name[display_name_len]  UTF-8
```

No common version produces `PROTOCOL_MISMATCH` and leaves the link unprivileged.
HELLO is required after every connection before any other application message.

## Pairing and authorization

Pairing uses LE Secure Connections with a runtime six-digit passkey displayed by the
ESP for a limited time. Successful bonding creates a random 64-bit `association_id`
stored in NVS and Android storage. The passkey is never stored.

Privileged/application-authoritative messages are `TIME_SYNC`, `RIDE_LIST_REQUEST`,
`RIDE_DOWNLOAD_REQUEST`, `FILE_ACK`, `MEDIA_STATE`, `NAV_STATE`, `CONFIG_GET` and
`CONFIG_SET`. They require:

1. encrypted bonded BLE link;
2. successful HELLO;
3. matching nonzero association ID.

Failed checks return `NOT_AUTHORIZED`. Protocol 1 still carries one association ID per
HELLO/session, but firmware keeps a bounded registry of up to four such IDs. Each bonded
phone must present its own exact nonzero value. Cancel closes only the current pairing
window; the on-device `Forget all` action clears the complete ESP registry and controller
bonds, after which each Android companion must pair again.

## Core payloads

TIME_SYNC:

```text
i64 unix_time_ms
i32 utc_offset_seconds
u8 timezone_len
u8 timezone_id[timezone_len]     e.g. Europe/Moscow
```

LIVE_TELEMETRY:

```text
u8 ride_state          0 IDLE, 1 RIDING, 2 PAUSED, 3 FINISHED
u8 motion_state        0 MOVING, 1 AUTO_PAUSED
u8 battery_percent
u8 sd_state            0 MISSING, 1 READY, 2 ERROR, 3 USB_OWNED
f32 speed_kmh
f32 distance_m
f32 average_speed_kmh
f32 max_speed_kmh
u64 moving_time_ms
u64 elapsed_time_ms
u32 pulse_count
u32 ride_id
```

RIDE_EVENT:

```text
u8 event               0 STARTED, 1 PAUSED, 2 RESUMED, 3 FINISHED
u32 ride_id
u64 ride_elapsed_ms
```

DEVICE_EVENT:

```text
u8 event               0 SD_ERROR, 1 LOW_BATTERY, 2 RAIN_LOCK_CHANGED
u32 detail
```

String payloads use a one-byte byte length and valid UTF-8, are capped per message, and
are deterministically truncated at a UTF-8 boundary.

MEDIA_STATE:

```text
u8 available
u8 playing
u32 supported_actions   bit 0 PLAY, 1 PAUSE, 2 TOGGLE,
                        bit 3 NEXT, 4 PREVIOUS, 5 SEEK
u64 duration_ms
u64 position_ms
u8 player_len
u8 player[player_len]   maximum 32 UTF-8 bytes
u8 title_len
u8 title[title_len]     maximum 64 UTF-8 bytes
u8 artist_len
u8 artist[artist_len]   maximum 64 UTF-8 bytes
```

MEDIA_ACTION:

```text
u8 action               0 PLAY, 1 PAUSE, 2 TOGGLE,
                        3 NEXT, 4 PREVIOUS, 5 SEEK
u64 position_ms         used by SEEK, zero otherwise
```

NAV_STATE:

```text
u8 available
u8 lifecycle            0 INACTIVE, 1 STARTING, 2 NAVIGATING,
                        3 REROUTING, 4 ARRIVED, 5 ERROR
u8 maneuver
u8 next_maneuver
u32 distance_to_maneuver_m
u32 next_distance_m
u32 remaining_distance_m
i64 eta_utc_ms
u8 street_len
u8 street[street_len]   maximum 64 UTF-8 bytes
```

Maneuvers are `0 STRAIGHT`, `1 TURN_LEFT`, `2 TURN_RIGHT`, `3 SLIGHT_LEFT`,
`4 SLIGHT_RIGHT`, `5 SHARP_LEFT`, `6 SHARP_RIGHT`, `7 UTURN`, `8 ROUNDABOUT`,
`9 ROUNDABOUT_EXIT`, `10 DESTINATION`, `255 UNKNOWN`.

Configuration values use:

```text
CONFIG_GET:    u16 key
CONFIG_VALUE:  u16 key, u8 value_type, u32 value_bits
CONFIG_SET:    u16 key, u8 value_type, u32 value_bits
CONFIG_RESULT: u16 key, u8 result
```

Value types are `1 BOOLEAN`, `2 U32`, `3 FLOAT32`. Keys are:

```text
1 wheel_circumference_m   FLOAT32, 0.5 .. 3.5
2 stop_threshold_kmh      FLOAT32, 0.5 .. 15.0
3 auto_pause_enabled      BOOLEAN
4 auto_pause_delay_ms     U32, 1000 .. 60000
5 log_sample_interval_ms  U32, 250 .. 10000
6 graph_window_seconds    U32, 10 .. 300
```

Result codes are `0 OK`, `1 UNKNOWN_KEY`, `2 WRONG_TYPE`, `3 INVALID_VALUE`,
`4 STORAGE_UNAVAILABLE`, `5 STORAGE_BUSY_USB`. Firmware rejects changes during an
active/manual-paused ride and exposes no hardware/GPIO key.

## Errors

```text
1 MALFORMED_FRAME
2 UNSUPPORTED_VERSION
3 CRC_MISMATCH
4 UNSUPPORTED_MESSAGE
5 NOT_AUTHORIZED
6 INVALID_STATE
7 INVALID_VALUE
8 STORAGE_UNAVAILABLE
9 STORAGE_BUSY_USB
10 RIDE_ACTIVE
11 NOT_FOUND
12 TRANSFER_TIMEOUT
13 INTEGRITY_FAILED
14 BUSY
15 PROTOCOL_MISMATCH
```

ERROR payload:

```text
u16 error_code
u8 rejected_message_type
u16 rejected_sequence
u8 detail_len
u8 detail[detail_len]
```

Protocol parsing errors never affect the ride core.

## Test vectors

Canonical vectors live in `protocol_test_vectors.json`. Kotlin tests load it directly;
the PlatformIO pre-build generator creates the C++ header used by embedded Unity tests
from the same JSON. The suite includes a valid frame, invalid magic, invalid version,
CRC mismatch, partial input, concatenated frames and maximum configured payload.
