#pragma once

#include <Arduino.h>

class BatteryMonitor {
 public:
  bool begin();
  bool enabled() const { return false; }
  int adcPin() const { return -1; }
  String statusText() const { return "Battery: N/A"; }
  String diagnosticText() const;
};
