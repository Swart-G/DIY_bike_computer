# Ride synchronization contract

Ride sync is optional, resumable and subordinate to ride logging and USB ownership.

## Preconditions

Automatic sync may run only when:

```text
BLE state is READY
ride state is not RIDING
USB MSC is inactive
SD is mounted and firmware-owned
association is authorized
```

PAUSED is still an active/open ride, so manifest and bulk file access are deferred until
Finish. Active/open ride files are never downloaded.

## Manifest discovery

Android sends `RIDE_LIST_REQUEST` with its last known global revision. ESP enumerates
ride directories through `RideRepository` and emits one `RIDE_MANIFEST` per ride,
followed by `RIDE_LIST_END`.

Manifest payload:

```text
u32 ride_id
u8 format_version
u8 finished
i64 started_at_utc_ms      -1 when unavailable
i64 finished_at_utc_ms     -1 when unavailable
f32 distance_m
u64 duration_ms
u32 sync_revision
u32 total_size
u8 file_count
repeat file_count:
  u8 file_id
  u32 size
  u32 crc32
```

Protocol-1 file IDs are fixed:

```text
1 meta.json
2 samples.csv
3 events.csv
4 summary.json
```

The phone never supplies an SD path. ESP maps `(ride_id, file_id)` to an enumerated,
validated finished ride and rejects unknown IDs. This prevents traversal and arbitrary
file reads.

Old v1 rides without a usable `ride_id` receive a stable synthetic Android ID derived
from device ID, normalized ride folder, metadata and file hash. The SD data is not
renamed or rewritten.

## Download

Android sends:

```text
RIDE_DOWNLOAD_REQUEST:
u32 ride_id
u8 file_id
u32 resume_offset
u32 expected_crc32       0 if not known
```

ESP validates state and replies:

```text
FILE_BEGIN:
u32 ride_id
u8 file_id
u32 total_size
u32 accepted_offset
u32 file_crc32
u16 chunk_data_max
```

Then ESP sends one bounded chunk at a time:

```text
FILE_CHUNK:
u32 ride_id
u8 file_id
u32 offset
u16 chunk_sequence
u16 data_length
u8 data[data_length]
```

Android writes to a private `.part` file, flushes durable progress as appropriate, and
acknowledges:

```text
FILE_ACK:
u32 ride_id
u8 file_id
u32 next_offset
u16 chunk_sequence
u8 status        0 OK, 1 RETRY, 2 CANCEL
```

ESP sends the next chunk only after a matching ACK. Offset and sequence mismatches are
rejected; `RETRY` retransmits the current chunk. Transfer timeout cancels only the sync
session.

Chunk data size is:

```text
min(implementation_limit,
    negotiated_att_mtu - 3 - protocol_frame_overhead - file_chunk_fields)
```

It is never hardcoded to 512 bytes.

At EOF:

```text
FILE_END:
u32 ride_id
u8 file_id
u32 total_size
u32 crc32
```

CRC32 is ISO-HDLC (`poly 0x04C11DB7`, reflected representation `0xEDB88320`,
init/xor-out `0xFFFFFFFF`). Frame integrity remains CRC16.

Android verifies length and whole-file CRC32 before atomically renaming `.part`. A ride
is marked synced only after every manifest file passes integrity and import commits.

## Resume

Room stores each file's verified manifest CRC/size and the durable `.part` length. After
reconnect Android repeats the request with that offset. ESP may round/reject an offset
greater than current file size; otherwise it accepts the exact offset. Already verified
files are skipped.

If the manifest revision, file size or CRC changes, Android discards only that stale
partial file and restarts it. Other verified files remain.

## Scheduling and fairness

`RideSyncManager` is a nonblocking state machine:

```text
IDLE -> LISTING -> WAIT_REQUEST -> FILE_BEGIN -> SEND_CHUNK
     -> WAIT_ACK -> FILE_END -> WAIT_REQUEST -> COMPLETE
any state -> CANCELLED/ERROR -> IDLE
```

Only one file and one unacknowledged chunk are active. Hall/ride/logging work always runs
before a sync step. Telemetry and immediate ride/device events are not starved by bulk
transfer.

## Failure behavior

- BLE loss: keep Android partial offset; firmware closes the sync file and returns idle.
- SD removal/read error: cancel with `STORAGE_UNAVAILABLE`; ride core continues.
- USB takeover: close transfer before ownership change and report `STORAGE_BUSY_USB`.
- ride becomes RIDING: cancel/defer bulk transfer with `RIDE_ACTIVE`.
- CRC mismatch: Android requests that file again; never mark the ride synced.
- duplicate manifest: Room upserts by `(device_id, stable_ride_id)` and revision.
- malformed request: protocol error only; no SD path is opened.
