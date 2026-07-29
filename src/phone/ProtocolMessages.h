#pragma once

#include <Arduino.h>

namespace bikeproto {

static constexpr uint8_t kProtocolVersion = 1;
static constexpr uint16_t kMagic = 0x4342;
static constexpr uint16_t kHeaderBytes = 9;
static constexpr uint16_t kFrameOverheadBytes = 11;
static constexpr uint16_t kMaximumPayloadBytes = 503;
static constexpr uint16_t kMaximumFrameBytes =
    kMaximumPayloadBytes + kFrameOverheadBytes;

static constexpr const char* kServiceUuid =
    "f5c10000-7d2a-4b8e-9c31-5a7e2d9b0001";
static constexpr const char* kDeviceInfoUuid =
    "f5c10001-7d2a-4b8e-9c31-5a7e2d9b0001";
static constexpr const char* kRxUuid =
    "f5c10002-7d2a-4b8e-9c31-5a7e2d9b0001";
static constexpr const char* kTxUuid =
    "f5c10003-7d2a-4b8e-9c31-5a7e2d9b0001";

enum class MessageType : uint8_t {
  Hello = 0x01,
  HelloAck = 0x02,
  Error = 0x03,
  Ping = 0x04,
  Pong = 0x05,
  TimeSync = 0x10,
  LiveTelemetry = 0x20,
  RideEvent = 0x21,
  DeviceEvent = 0x22,
  RideListRequest = 0x30,
  RideManifest = 0x31,
  RideListEnd = 0x32,
  RideDownloadRequest = 0x33,
  FileBegin = 0x34,
  FileChunk = 0x35,
  FileAck = 0x36,
  FileEnd = 0x37,
  TransferCancel = 0x38,
  MediaState = 0x40,
  MediaAction = 0x41,
  NavigationState = 0x50,
  ConfigGet = 0x60,
  ConfigValue = 0x61,
  ConfigSet = 0x62,
  ConfigResult = 0x63,
};

enum FrameFlags : uint8_t {
  AckRequired = 0x01,
  Response = 0x02,
  Error = 0x04,
  More = 0x08,
  Privileged = 0x10,
};

enum Capability : uint32_t {
  LiveTelemetry = 1UL << 0,
  RideSync = 1UL << 1,
  MediaControl = 1UL << 2,
  GpsAssist = 1UL << 3,
  Navigation = 1UL << 4,
  ConfigSync = 1UL << 5,
};

static constexpr uint32_t kImplementedCapabilities =
    Capability::LiveTelemetry | Capability::RideSync | Capability::MediaControl |
    Capability::GpsAssist | Capability::Navigation | Capability::ConfigSync;

enum class ErrorCode : uint16_t {
  MalformedFrame = 1,
  UnsupportedVersion = 2,
  CrcMismatch = 3,
  UnsupportedMessage = 4,
  NotAuthorized = 5,
  InvalidState = 6,
  InvalidValue = 7,
  StorageUnavailable = 8,
  StorageBusyUsb = 9,
  RideActive = 10,
  NotFound = 11,
  TransferTimeout = 12,
  IntegrityFailed = 13,
  Busy = 14,
  ProtocolMismatch = 15,
};

}  // namespace bikeproto
