#include "battery/BatteryMonitor.h"

#include "config/hardware_config.h"

bool BatteryMonitor::begin() {
  return hw::BATTERY_MONITOR_ENABLED && hw::BATTERY_ADC_PIN >= 0;
}

String BatteryMonitor::diagnosticText() const {
  return "Battery monitor: disabled\nADC pin: not configured\n";
}
