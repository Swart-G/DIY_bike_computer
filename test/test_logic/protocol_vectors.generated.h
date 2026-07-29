// Generated from docs/protocol_test_vectors.json. Do not edit.
#pragma once

#include <stdint.h>

namespace protocolvectors {
static constexpr uint8_t kValidEmptyHello[] = {0x42, 0x43, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x34};
static constexpr uint8_t kValidPing[] = {0x42, 0x43, 0x01, 0x04, 0x00, 0x2a, 0x00, 0x04, 0x00, 0x78, 0x56, 0x34, 0x12, 0xf6, 0x69};
static constexpr uint16_t kMaximumPayloadBytes =
    503;
static constexpr uint16_t kMaximumFrameBytes =
    514;
static constexpr uint16_t kMaximumPayloadCrc =
    0xD5CE;
}  // namespace protocolvectors
