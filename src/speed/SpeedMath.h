#pragma once
#include <stdint.h>
namespace speedmath {
inline float kmhFromIntervalUs(uint64_t intervalUs, float circumferenceM, uint8_t pulsesPerRevolution) {
  return intervalUs && pulsesPerRevolution ? (circumferenceM / pulsesPerRevolution) * 3600000.0f / intervalUs : 0.0f;
}
}
