#pragma once

#include <Arduino.h>

#include "phone/ProtocolMessages.h"

namespace bikeproto {

struct Frame {
  MessageType type = MessageType::Error;
  uint8_t flags = 0;
  uint16_t sequence = 0;
  uint16_t payloadLength = 0;
  uint8_t payload[kMaximumPayloadBytes] = {0};
};

enum class DecodeResult : uint8_t {
  FrameReady,
  NeedMoreData,
  InvalidMagic,
  UnsupportedVersion,
  PayloadTooLarge,
  CrcMismatch,
  BufferOverflow,
};

class Codec {
 public:
  static uint16_t crc16(const uint8_t* data, size_t length);
  static bool encode(MessageType type, uint8_t flags, uint16_t sequence,
                     const uint8_t* payload, uint16_t payloadLength,
                     uint8_t* output, size_t outputCapacity, uint16_t& outputLength);
};

class Decoder {
 public:
  DecodeResult feed(const uint8_t* data, size_t length);
  DecodeResult next(Frame& frame);
  void reset();
  size_t bufferedBytes() const { return bufferedBytes_; }

 private:
  void discard(size_t count);
  uint8_t buffer_[kMaximumFrameBytes * 2] = {0};
  size_t bufferedBytes_ = 0;
  bool overflowPending_ = false;
};

inline uint16_t readU16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8);
}
inline uint32_t readU32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}
inline uint64_t readU64(const uint8_t* data) {
  uint64_t value = 0;
  for (uint8_t i = 0; i < 8; ++i) {
    value |= static_cast<uint64_t>(data[i]) << (i * 8);
  }
  return value;
}
inline void writeU16(uint8_t* data, uint16_t value) {
  data[0] = value & 0xFF;
  data[1] = value >> 8;
}
inline void writeU32(uint8_t* data, uint32_t value) {
  data[0] = value & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  data[2] = (value >> 16) & 0xFF;
  data[3] = value >> 24;
}
inline void writeU64(uint8_t* data, uint64_t value) {
  for (uint8_t i = 0; i < 8; ++i) data[i] = (value >> (i * 8)) & 0xFF;
}
inline void writeFloat(uint8_t* data, float value) {
  static_assert(sizeof(float) == sizeof(uint32_t), "Protocol requires IEEE-754 float32");
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  writeU32(data, bits);
}

}  // namespace bikeproto
