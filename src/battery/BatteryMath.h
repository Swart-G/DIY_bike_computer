#pragma once
#include <stdint.h>
namespace batterymath {
inline float calibratedVoltage(float adcVoltage, float dividerRatio, float calibration) { return adcVoltage * dividerRatio * calibration; }
inline float percentFromVoltage(float v) {
  static constexpr float volts[] = {3.20f,3.35f,3.50f,3.60f,3.70f,3.75f,3.80f,3.85f,3.90f,4.00f,4.10f,4.20f};
  static constexpr float pct[] = {0,3,8,14,25,35,45,55,65,78,90,100};
  if(v<=volts[0]) return 0; for(uint8_t i=1;i<12;++i) if(v<=volts[i]) return pct[i-1]+(v-volts[i-1])*(pct[i]-pct[i-1])/(volts[i]-volts[i-1]); return 100;
}
}
