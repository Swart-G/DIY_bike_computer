#include "phone/BikeProtocol.h"

#include <cstring>

namespace bikeproto {

uint16_t Codec::crc16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = crc & 0x8000 ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                         : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

bool Codec::encode(MessageType type, uint8_t flags, uint16_t sequence,
                   const uint8_t* payload, uint16_t payloadLength,
                   uint8_t* output, size_t outputCapacity, uint16_t& outputLength) {
  outputLength = 0;
  if (!output || payloadLength > kMaximumPayloadBytes ||
      outputCapacity < payloadLength + kFrameOverheadBytes ||
      (payloadLength && !payload)) {
    return false;
  }
  output[0] = 'B';
  output[1] = 'C';
  output[2] = kProtocolVersion;
  output[3] = static_cast<uint8_t>(type);
  output[4] = flags;
  writeU16(output + 5, sequence);
  writeU16(output + 7, payloadLength);
  if (payloadLength) memcpy(output + kHeaderBytes, payload, payloadLength);
  const uint16_t crc = crc16(output, kHeaderBytes + payloadLength);
  writeU16(output + kHeaderBytes + payloadLength, crc);
  outputLength = payloadLength + kFrameOverheadBytes;
  return true;
}

DecodeResult Decoder::feed(const uint8_t* data, size_t length) {
  if (!data && length) return DecodeResult::BufferOverflow;
  if (length > sizeof(buffer_) - bufferedBytes_) {
    reset();
    overflowPending_ = true;
    return DecodeResult::BufferOverflow;
  }
  if (length) {
    memcpy(buffer_ + bufferedBytes_, data, length);
    bufferedBytes_ += length;
  }
  return DecodeResult::NeedMoreData;
}

DecodeResult Decoder::next(Frame& frame) {
  if (overflowPending_) {
    overflowPending_ = false;
    return DecodeResult::BufferOverflow;
  }
  if (bufferedBytes_ < 2) return DecodeResult::NeedMoreData;

  size_t magicOffset = 0;
  while (magicOffset + 1 < bufferedBytes_ &&
         (buffer_[magicOffset] != 'B' || buffer_[magicOffset + 1] != 'C')) {
    ++magicOffset;
  }
  if (magicOffset > 0) {
    discard(magicOffset);
    return DecodeResult::InvalidMagic;
  }
  if (bufferedBytes_ < kHeaderBytes) return DecodeResult::NeedMoreData;
  if (buffer_[2] != kProtocolVersion) {
    discard(2);
    return DecodeResult::UnsupportedVersion;
  }

  const uint16_t payloadLength = readU16(buffer_ + 7);
  if (payloadLength > kMaximumPayloadBytes) {
    discard(2);
    return DecodeResult::PayloadTooLarge;
  }
  const size_t frameLength = payloadLength + kFrameOverheadBytes;
  if (bufferedBytes_ < frameLength) return DecodeResult::NeedMoreData;
  const uint16_t expected = readU16(buffer_ + kHeaderBytes + payloadLength);
  const uint16_t actual = Codec::crc16(buffer_, kHeaderBytes + payloadLength);
  if (expected != actual) {
    discard(2);
    return DecodeResult::CrcMismatch;
  }

  frame.type = static_cast<MessageType>(buffer_[3]);
  frame.flags = buffer_[4];
  frame.sequence = readU16(buffer_ + 5);
  frame.payloadLength = payloadLength;
  if (payloadLength) memcpy(frame.payload, buffer_ + kHeaderBytes, payloadLength);
  discard(frameLength);
  return DecodeResult::FrameReady;
}

void Decoder::reset() {
  bufferedBytes_ = 0;
  overflowPending_ = false;
}

void Decoder::discard(size_t count) {
  if (count >= bufferedBytes_) {
    bufferedBytes_ = 0;
    return;
  }
  memmove(buffer_, buffer_ + count, bufferedBytes_ - count);
  bufferedBytes_ -= count;
}

}  // namespace bikeproto
