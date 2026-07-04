#include <Arduino.h>

#include "battery/BatteryMonitor.h"
#include "config/app_config.h"
#include "display/DisplayManager.h"
#include "speed/HallSensor.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedCalculator.h"
#include "storage/StorageManager.h"
#include "touch/TouchManager.h"
#include "ui/UiApp.h"
#include "usb/UsbMassStorageManager.h"

namespace {

app::AppSettings g_settings;
DisplayManager g_display;
TouchManager g_touch;
StorageManager g_storage;
UsbMassStorageManager g_usb;
HallSensor g_sensor;
SpeedCalculator g_speed;
RideStateMachine g_ride;
BatteryMonitor g_battery;
UiApp g_ui;

void printBootInfo() {
  Serial.println();
  Serial.println("Bike Speedometer boot");
  Serial.print("Firmware: ");
  Serial.println(app::FIRMWARE_VERSION);
  Serial.print("Board: ");
  Serial.println(app::BOARD_NAME);
  Serial.print("Flash bytes: ");
  Serial.println(ESP.getFlashChipSize());
  Serial.print("PSRAM found: ");
  Serial.println(psramFound() ? "yes" : "no");
  Serial.print("PSRAM bytes: ");
  Serial.println(psramFound() ? ESP.getPsramSize() : 0);
  Serial.print("CPU MHz: ");
  Serial.println(ESP.getCpuFreqMHz());
}

void logInitResult(const char* name, bool ok) {
  Serial.print(name);
  Serial.print(": ");
  Serial.println(ok ? "OK" : "FAILED");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  printBootInfo();

  g_display.begin(g_settings.displayBrightnessPercent);
  g_display.showBoot("Initializing hardware", app::FIRMWARE_VERSION);
  logInitResult("Display", g_display.isReady());
  logInitResult("Display framebuffer", g_display.frameBufferReady());
  logInitResult("Display transfer buffer", g_display.frameTransferBufferReady());

  const bool touchOk = g_touch.begin();
  logInitResult("Touch FT6336", touchOk);

  g_display.showBoot("Initializing SD", "No SD is allowed");
  const bool sdOk = g_storage.begin();
  logInitResult("SD", sdOk);

  if (sdOk) {
    String configError;
    const bool configOk = g_storage.loadSettings(g_settings, configError);
    logInitResult("Config", configOk);
    if (configError.length() > 0) {
      Serial.print("Config note: ");
      Serial.println(configError);
    }
    g_display.setBrightness(g_settings.displayBrightnessPercent);
  } else {
    app::validateSettings(g_settings);
  }

  const bool sensorOk = g_sensor.begin(g_settings);
  logInitResult("Hall sensor GPIO4", sensorOk);

  const bool batteryOk = g_battery.begin();
  Serial.print("Battery monitor: ");
  Serial.println(batteryOk ? "enabled" : "disabled");

  g_speed.reset();
  g_ride.begin(&g_settings);
  g_ui.begin(g_display, g_touch, g_storage, g_usb, g_sensor, g_speed, g_ride, g_battery, g_settings);

  Serial.println("Boot complete");
}

void loop() {
  g_ui.loop();
  yield();
}
