Import("env")

import json
from pathlib import Path

project = Path(env.subst("$PROJECT_DIR"))
source = project / "docs" / "protocol_test_vectors.json"
target = project / "test" / "test_logic" / "protocol_vectors.generated.h"
document = json.loads(source.read_text(encoding="utf-8"))
vectors = {item["name"]: item for item in document["vectors"]}


def byte_array(hex_value):
    values = bytes.fromhex(hex_value)
    return ", ".join(f"0x{value:02x}" for value in values)


hello = vectors["valid_empty_hello"]
ping = vectors["valid_ping_payload"]
maximum = vectors["maximum_payload"]
content = f"""// Generated from docs/protocol_test_vectors.json. Do not edit.
#pragma once

#include <stdint.h>

namespace protocolvectors {{
static constexpr uint8_t kValidEmptyHello[] = {{{byte_array(hello["input_hex"])}}};
static constexpr uint8_t kValidPing[] = {{{byte_array(ping["input_hex"])}}};
static constexpr uint16_t kMaximumPayloadBytes =
    {document["codec_limits"]["maximum_payload_bytes"]};
static constexpr uint16_t kMaximumFrameBytes =
    {document["codec_limits"]["maximum_frame_bytes"]};
static constexpr uint16_t kMaximumPayloadCrc =
    {maximum["crc16"]};
}}  // namespace protocolvectors
"""
if not target.exists() or target.read_text(encoding="utf-8") != content:
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")
