#include <Arduino.h>

#include "battery/BatteryMonitor.h"
#include "config/app_config.h"
#include "config/ConfigSyncManager.h"
#include "config/hardware_config.h"
#include "display/DisplayManager.h"
#include "dev/DevMonitor.h"
#include "led/SpeedTrendLed.h"
#include "phone/PhoneLinkManager.h"
#include "speed/HallSensor.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedCalculator.h"
#include "storage/StorageManager.h"
#include "storage/RideLogger.h"
#include "storage/RideRepository.h"
#include "sync/RideSyncManager.h"
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
SpeedTrendLed g_speedTrendLed;
RideStateMachine g_ride;
BatteryMonitor g_battery;
RideLogger g_rideLogger;
RideRepository g_rideRepository;
PhoneLinkManager g_phone;
RideSyncManager g_rideSync;
ConfigSyncManager g_configSync;
UiApp g_ui;
DevMonitor g_devMonitor;
uint32_t g_appliedClockGeneration = 0;

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

void bootInitResult(const char* name, bool ok, const String& detail = String()) {
  logInitResult(name, ok);
  g_display.addBootLog(name, ok, detail);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
  printBootInfo();

  g_display.begin(app::DISPLAY_FIXED_BRIGHTNESS_PERCENT);
  g_display.resetBoot(app::FIRMWARE_VERSION);
  g_display.animateBoot(170);
  bootInitResult("Display", g_display.isReady(), "write only");
  bootInitResult("Framebuffer", g_display.frameBufferReady(),
                 g_display.frameTransferBufferReady() ? "DMA-safe copy" : "fallback");

  const bool touchOk = g_touch.begin();
  bootInitResult("Touch FT6336", touchOk);

  g_display.showBoot("Initializing SD", "No SD is allowed");
  const bool sdOk = g_storage.begin();
  bootInitResult("SD card", sdOk, sdOk ? "mounted" : "no-SD mode");

  String configError;
  const bool configOk = g_storage.loadSettings(g_settings, configError);
  bootInitResult("Configuration", configOk,
                 configError.length() ? configError : String("loaded"));
  if (configError.length() > 0) {
    Serial.print("Config note: ");
    Serial.println(configError);
  }

  const bool sensorOk = g_sensor.begin(g_settings);
  const String hallLabel = String("Hall sensor GPIO") + String(hw::PIN_HALL_SENSOR);
  bootInitResult(hallLabel.c_str(), sensorOk);

  const bool batteryOk = g_battery.begin(g_settings);
  Serial.print("Battery monitor: ");
  Serial.println(batteryOk ? "enabled" : "initialization failed");
  g_display.addBootLog("Battery", batteryOk);

  const bool bleOk = g_phone.begin();
  bootInitResult("BLE companion", bleOk);
  g_display.animateBoot(180);

  g_rideLogger.setClock(&g_phone.clock());
  g_rideSync.begin(g_storage, g_rideRepository);
  g_phone.attachSync(g_rideSync);
  g_configSync.begin(g_storage, g_sensor, g_ride, g_settings);
  g_phone.attachConfig(g_configSync);

  g_speed.reset();
  g_speedTrendLed.begin(g_settings);
  g_ride.begin(&g_settings);
  g_ui.begin(g_display, g_touch, g_storage, g_usb, g_sensor, g_speed,
             g_speedTrendLed, g_ride, g_battery,
             g_rideLogger, g_rideRepository, g_phone, g_settings,
             g_devMonitor);

  g_display.addBootLog("Ready", true, "opening UI");
  g_display.animateBoot(220);
  g_devMonitor.attach(g_display, g_touch, g_storage, g_usb, g_sensor, g_speed,
                      g_speedTrendLed, g_ride, g_battery, g_rideLogger,
                      g_phone, g_ui, g_rideSync, g_configSync);
  Serial.println("Boot complete");
}

void loop() {
  g_devMonitor.noteLoopStart(micros());
  const uint32_t nowMs = millis();
  g_battery.setUsbConnected(g_usb.dataConnected(), nowMs);
  g_phone.update(nowMs);
  if (g_phone.clock().generation() != g_appliedClockGeneration) {
    g_appliedClockGeneration = g_phone.clock().generation();
    String clockError;
    if (!g_rideLogger.applyClockSync(g_storage, clockError) &&
        clockError.length()) {
      Serial.print("[STORAGE] clock metadata update failed: ");
      Serial.println(clockError);
    }
  }
  g_ui.loop();
  const RideStats& stats = g_ride.stats();
  LiveTelemetryInput telemetry;
  switch (g_ride.state()) {
    case RideState::RIDING: telemetry.rideState = 1; break;
    case RideState::PAUSED: telemetry.rideState = 2; break;
    case RideState::FINISHED: telemetry.rideState = 3; break;
    case RideState::IDLE: default: telemetry.rideState = 0; break;
  }
  telemetry.motionState =
      g_ride.motionState() == MotionState::AUTO_PAUSED ? 1 : 0;
  telemetry.batteryPercent = g_battery.percent();
  telemetry.sdState = g_usb.active() ? 3 : (g_storage.sdAvailable() ? 1 : 0);
  telemetry.speedKmh = g_speed.currentKmh();
  telemetry.distanceM = stats.distanceM;
  telemetry.averageSpeedKmh = stats.averageMovingSpeedKmh;
  telemetry.maxSpeedKmh = stats.maxSpeedKmh;
  telemetry.movingTimeMs = stats.movingMs;
  telemetry.elapsedTimeMs = stats.elapsedMs;
  telemetry.pulseCount = stats.pulseCount;
  telemetry.rideId = g_rideLogger.rideId();
  telemetry.rainLocked = g_ui.rainLocked();
  telemetry.batteryLow = g_battery.state() == BatteryState::Low ||
                         g_battery.state() == BatteryState::Critical;
  g_rideSync.setRuntimeState(telemetry.rideState, g_usb.active());
  g_phone.updateLiveData(telemetry, nowMs);
  g_devMonitor.update(nowMs);
  yield();
}
