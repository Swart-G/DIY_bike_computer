#include "ui/UiApp.h"

#include "config/hardware_config.h"

namespace {

constexpr int16_t kBackX = 376;
constexpr int16_t kBackY = 270;
constexpr int16_t kBackW = 92;
constexpr int16_t kBackH = 40;

String flashSizeText() {
  return String(static_cast<uint32_t>(ESP.getFlashChipSize() / (1024UL * 1024UL))) + " MB";
}

String psramText() {
  if (!psramFound()) {
    return "not found";
  }
  return String(static_cast<uint32_t>(ESP.getPsramSize() / (1024UL * 1024UL))) + " MB";
}

}  // namespace

void UiApp::begin(DisplayManager& display, TouchManager& touch, StorageManager& storage,
                  UsbMassStorageManager& usb, HallSensor& sensor, SpeedCalculator& speed,
                  RideStateMachine& ride, BatteryMonitor& battery, app::AppSettings& settings) {
  display_ = &display;
  touch_ = &touch;
  storage_ = &storage;
  usb_ = &usb;
  sensor_ = &sensor;
  speed_ = &speed;
  ride_ = &ride;
  battery_ = &battery;
  settings_ = &settings;

  String recoveryError;
  hasPendingRecovery_ = storage_->loadRecovery(pendingRecovery_, recoveryError);
  if (hasPendingRecovery_) {
    ride_->restorePaused(pendingRecovery_, millis(), sensor_->snapshot().pulseCount);
    enter(Screen::Recovery);
  } else if (!storage_->sdAvailable()) {
    enter(Screen::SdMissing);
  } else {
    enter(Screen::MainMenu);
  }
}

void UiApp::loop() {
  const uint32_t now = millis();
  touch_->update();
  updateModel(now);

  const bool touchedNow = touch_->touched();
  if (screen_ == Screen::PaintTest && touchedNow) {
    handlePaint();
  } else if (screen_ == Screen::PaintTest && !touchedNow) {
    lastPaintValid_ = false;
  }

  if (touchedNow && !wasTouched_) {
    handleTap(touch_->x(), touch_->y());
  }
  wasTouched_ = touchedNow;

  const bool dynamicScreen = screen_ == Screen::Ride || screen_ == Screen::TouchRawTest ||
                             screen_ == Screen::SensorTest || screen_ == Screen::SystemInfo;
  if (dirty_ || (dynamicScreen && now - lastUiDrawMs_ >= settings_->uiUpdateIntervalMs)) {
    draw();
    dirty_ = false;
    lastUiDrawMs_ = now;
  }
}

void UiApp::enter(Screen screen) {
  screen_ = screen;
  dirty_ = true;
  lastPaintValid_ = false;
  if (screen == Screen::SdTest) {
    sdTestRun_ = false;
    sdTestResult_ = SdTestResult();
  }
}

void UiApp::updateModel(uint32_t nowMs) {
  sensorSnapshot_ = sensor_->snapshot();
  speed_->update(sensorSnapshot_, *settings_, nowMs);
  ride_->update(nowMs, speed_->currentKmh(), sensorSnapshot_.pulseCount);
  recordGraphSample(nowMs);
  saveRecoveryIfNeeded(nowMs, false);
}

void UiApp::handleTap(int16_t x, int16_t y) {
  switch (screen_) {
    case Screen::SdMissing:
      if (hit(x, y, 70, 180, 150, 54)) {
        if (storage_->retry()) {
          String error;
          storage_->loadSettings(*settings_, error);
          display_->setBrightness(settings_->displayBrightnessPercent);
          enter(Screen::MainMenu);
        } else {
          lastMessage_ = "SD retry failed";
          dirty_ = true;
        }
      } else if (hit(x, y, 250, 180, 170, 54)) {
        storage_->continueWithoutSaving();
        enter(Screen::MainMenu);
      }
      break;

    case Screen::Recovery:
      if (hit(x, y, 52, 204, 110, 48)) {
        ride_->resume(millis());
        saveRecoveryIfNeeded(millis(), true);
        enter(Screen::Ride);
      } else if (hit(x, y, 185, 204, 110, 48)) {
        ride_->finish(millis());
        clearRecovery();
        enter(Screen::Ride);
      } else if (hit(x, y, 318, 204, 110, 48)) {
        clearRecovery();
        ride_->newRide(millis(), sensorSnapshot_.pulseCount);
        enter(Screen::MainMenu);
      }
      break;

    case Screen::MainMenu:
      if (hit(x, y, 28, 58, 200, 48)) {
        enter(Screen::Ride);
      } else if (hit(x, y, 252, 58, 200, 48)) {
        enter(Screen::Diagnostics);
      } else if (hit(x, y, 28, 122, 200, 48)) {
        enter(Screen::Settings);
      } else if (hit(x, y, 252, 122, 200, 48) && storage_->sdAvailable()) {
        startUsbMode();
        enter(Screen::UsbStorage);
      } else if (hit(x, y, 28, 186, 200, 48)) {
        enter(Screen::About);
      }
      break;

    case Screen::Diagnostics:
      if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        enter(Screen::MainMenu);
      } else if (hit(x, y, 18, 48, 210, 40)) {
        enter(Screen::DisplayTest);
      } else if (hit(x, y, 252, 48, 210, 40)) {
        enter(Screen::TouchRawTest);
      } else if (hit(x, y, 18, 100, 210, 40)) {
        enter(Screen::PaintTest);
      } else if (hit(x, y, 252, 100, 210, 40) && storage_->sdAvailable()) {
        enter(Screen::SdTest);
      } else if (hit(x, y, 18, 152, 210, 40) && storage_->sdAvailable()) {
        startUsbMode();
        enter(Screen::UsbStorage);
      } else if (hit(x, y, 252, 152, 210, 40)) {
        enter(Screen::SensorTest);
      } else if (hit(x, y, 18, 204, 210, 40)) {
        enter(Screen::BatteryTest);
      } else if (hit(x, y, 252, 204, 210, 40)) {
        enter(Screen::SystemInfo);
      }
      break;

    case Screen::DisplayTest:
    case Screen::TouchRawTest:
    case Screen::BatteryTest:
    case Screen::SystemInfo:
    case Screen::SensorTest:
      if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        enter(Screen::Diagnostics);
      }
      break;

    case Screen::About:
      if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        enter(Screen::MainMenu);
      }
      break;

    case Screen::PaintTest:
      if (hit(x, y, 14, 270, 92, 40)) {
        dirty_ = true;
      } else if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        enter(Screen::Diagnostics);
      }
      break;

    case Screen::SdTest:
      if (hit(x, y, 22, 270, 120, 40)) {
        sdTestResult_ = storage_->runSdTest();
        sdTestRun_ = true;
        dirty_ = true;
      } else if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        enter(Screen::Diagnostics);
      }
      break;

    case Screen::UsbStorage:
      if (hit(x, y, kBackX, kBackY, kBackW, kBackH) && !usb_->active()) {
        enter(Screen::MainMenu);
      }
      break;

    case Screen::Settings:
      if (hit(x, y, 24, 76, 46, 36)) {
        settings_->wheelCircumferenceM -= 0.005f;
      } else if (hit(x, y, 182, 76, 46, 36)) {
        settings_->wheelCircumferenceM += 0.005f;
      } else if (hit(x, y, 24, 132, 46, 36)) {
        settings_->stopThresholdKmh -= 0.5f;
      } else if (hit(x, y, 182, 132, 46, 36)) {
        settings_->stopThresholdKmh += 0.5f;
      } else if (hit(x, y, 24, 188, 46, 36)) {
        if (settings_->displayBrightnessPercent >= 10) {
          settings_->displayBrightnessPercent -= 10;
        }
      } else if (hit(x, y, 182, 188, 46, 36)) {
        settings_->displayBrightnessPercent += 10;
      } else if (hit(x, y, 270, 76, 170, 38)) {
        settings_->sensorInterruptMode =
            settings_->sensorInterruptMode == FALLING ? RISING
                                                      : (settings_->sensorInterruptMode == RISING ? CHANGE : FALLING);
      } else if (hit(x, y, 270, 132, 170, 38)) {
        settings_->sensorActiveLevel = settings_->sensorActiveLevel == LOW ? HIGH : LOW;
      } else if (hit(x, y, 270, 188, 170, 38)) {
        settings_->sensorPullupEnabled = !settings_->sensorPullupEnabled;
      } else if (hit(x, y, 156, 270, 110, 40)) {
        app::validateSettings(*settings_);
        display_->setBrightness(settings_->displayBrightnessPercent);
        sensor_->updateSettings(*settings_);
        String error;
        lastMessage_ = storage_->saveSettings(*settings_, error) ? "Settings saved" : error;
      } else if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        app::validateSettings(*settings_);
        display_->setBrightness(settings_->displayBrightnessPercent);
        sensor_->updateSettings(*settings_);
        enter(Screen::MainMenu);
        return;
      }
      app::validateSettings(*settings_);
      display_->setBrightness(settings_->displayBrightnessPercent);
      dirty_ = true;
      break;

    case Screen::Ride:
      if (hit(x, y, 398, 4, 76, 28)) {
        enter(Screen::MainMenu);
      } else if (hit(x, y, 18, 144, 52, 40)) {
        ridePage_ = ridePage_ == 0 ? 2 : ridePage_ - 1;
        dirty_ = true;
      } else if (hit(x, y, 410, 144, 52, 40)) {
        ridePage_ = (ridePage_ + 1) % 3;
        dirty_ = true;
      } else if (hit(x, y, 16, 266, 140, 46)) {
        if (ride_->state() == RideState::IDLE || ride_->state() == RideState::FINISHED) {
          ride_->start(millis(), sensorSnapshot_.pulseCount);
          saveRecoveryIfNeeded(millis(), true);
        }
        dirty_ = true;
      } else if (hit(x, y, 170, 266, 140, 46)) {
        if (ride_->state() == RideState::RIDING) {
          ride_->pause(millis());
        } else if (ride_->state() == RideState::PAUSED) {
          ride_->resume(millis());
        }
        saveRecoveryIfNeeded(millis(), true);
        dirty_ = true;
      } else if (hit(x, y, 324, 266, 140, 46)) {
        if (ride_->state() == RideState::RIDING || ride_->state() == RideState::PAUSED) {
          ride_->finish(millis());
          clearRecovery();
        } else if (ride_->state() == RideState::FINISHED) {
          ride_->newRide(millis(), sensorSnapshot_.pulseCount);
        }
        dirty_ = true;
      }
      break;
  }
}

void UiApp::handlePaint() {
  const int16_t x = touch_->x();
  const int16_t y = touch_->y();
  if (y < 42 || y > 258) {
    lastPaintValid_ = false;
    return;
  }
  TFT_eSPI& tft = display_->tft();
  if (lastPaintValid_) {
    tft.drawLine(lastPaintX_, lastPaintY_, x, y, TFT_YELLOW);
  } else {
    tft.fillCircle(x, y, 2, TFT_YELLOW);
  }
  lastPaintX_ = x;
  lastPaintY_ = y;
  lastPaintValid_ = true;
}

void UiApp::draw() {
  display_->beginFrame();
  switch (screen_) {
    case Screen::SdMissing:
      drawSdMissing();
      break;
    case Screen::Recovery:
      drawRecovery();
      break;
    case Screen::MainMenu:
      drawMainMenu();
      break;
    case Screen::Diagnostics:
      drawDiagnostics();
      break;
    case Screen::DisplayTest:
      drawDisplayTest();
      break;
    case Screen::TouchRawTest:
      drawTouchRawTest();
      break;
    case Screen::PaintTest:
      drawPaintTest();
      break;
    case Screen::SdTest:
      drawSdTest();
      break;
    case Screen::UsbStorage:
      drawUsbStorage();
      break;
    case Screen::SensorTest:
      drawSensorTest();
      break;
    case Screen::BatteryTest:
      drawBatteryTest();
      break;
    case Screen::SystemInfo:
      drawSystemInfo();
      break;
    case Screen::Settings:
      drawSettings();
      break;
    case Screen::About:
      drawAbout();
      break;
    case Screen::Ride:
      drawRide();
      break;
  }
  display_->commitFrame();
}

void UiApp::drawSdMissing() {
  display_->clear();
  display_->drawHeader("SD card not found", "NO SD");
  drawTextBlock("Ride logging disabled\n\nYou can retry SD init or continue without saving.",
                42, 74, 24, TFT_WHITE);
  if (lastMessage_.length()) {
    drawTextBlock(lastMessage_, 42, 146, 22, TFT_RED);
  }
  display_->drawButton(70, 180, 150, 54, "Retry", TFT_BLUE);
  display_->drawButton(250, 180, 170, 54, "Continue", TFT_DARKGREEN);
}

void UiApp::drawRecovery() {
  display_->clear();
  display_->drawHeader("Unfinished ride found", storage_->statusText());
  drawTextBlock("Recovered as paused\nDistance: " + String(ride_->stats().distanceM, 1) +
                    " m\nMoving: " + durationText(ride_->stats().movingMs),
                52, 74, 26, TFT_WHITE);
  display_->drawButton(52, 204, 110, 48, "Resume", TFT_DARKGREEN);
  display_->drawButton(185, 204, 110, 48, "Finish", TFT_ORANGE);
  display_->drawButton(318, 204, 110, 48, "Discard", TFT_RED);
}

void UiApp::drawMainMenu() {
  display_->clear();
  display_->drawHeader("Main Menu", storage_->statusText());
  display_->drawButton(28, 58, 200, 48, "Ride", TFT_DARKGREEN);
  display_->drawButton(252, 58, 200, 48, "Diagnostics", TFT_BLUE);
  display_->drawButton(28, 122, 200, 48, "Settings", TFT_PURPLE);
  display_->drawButton(252, 122, 200, 48, "USB Storage", TFT_ORANGE, TFT_BLACK,
                       storage_->sdAvailable());
  display_->drawButton(28, 186, 200, 48, "About", TFT_DARKGREY);
  drawTextBlock(battery_->statusText(), 256, 198, 22, TFT_LIGHTGREY);
}

void UiApp::drawDiagnostics() {
  display_->clear();
  display_->drawHeader("Diagnostics", storage_->statusText());
  display_->drawButton(18, 48, 210, 40, "Display test", TFT_BLUE);
  display_->drawButton(252, 48, 210, 40, "Touch raw test", TFT_BLUE);
  display_->drawButton(18, 100, 210, 40, "Paint test", TFT_DARKGREEN);
  display_->drawButton(252, 100, 210, 40, "SD test", TFT_DARKGREEN, TFT_WHITE,
                       storage_->sdAvailable());
  display_->drawButton(18, 152, 210, 40, "USB MSC test", TFT_ORANGE, TFT_BLACK,
                       storage_->sdAvailable());
  display_->drawButton(252, 152, 210, 40, "Sensor test", TFT_PURPLE);
  display_->drawButton(18, 204, 210, 40, "Battery test", TFT_DARKGREY);
  display_->drawButton(252, 204, 210, 40, "System info", TFT_DARKGREY);
  drawBackButton();
}

void UiApp::drawDisplayTest() {
  TFT_eSPI& tft = display_->tft();
  tft.fillScreen(TFT_BLACK);
  display_->drawHeader("Display test", "ST7796");
  const uint16_t colors[] = {TFT_BLACK, TFT_WHITE, TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN};
  for (uint8_t i = 0; i < 6; ++i) {
    tft.fillRect(20 + i * 74, 54, 64, 48, colors[i]);
    tft.drawRect(20 + i * 74, 54, 64, 48, TFT_WHITE);
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Text readability 1234567890", 22, 122, 2);
  tft.drawString("Orientation: landscape 480x320", 22, 148, 2);
  for (int i = 0; i < 8; ++i) {
    tft.drawLine(24, 190 + i * 6, 260 + i * 18, 246, TFT_YELLOW);
  }
  tft.drawRect(300, 130, 120, 74, TFT_GREEN);
  tft.drawRoundRect(316, 150, 120, 74, 10, TFT_RED);
  drawBackButton();
}

void UiApp::drawTouchRawTest() {
  display_->clear();
  display_->drawHeader("Touch raw test", touch_->isReady() ? "FT6336 OK" : "NO TOUCH");
  const TouchPoint& p = touch_->point();
  String text;
  text += "Touched: ";
  text += p.touched ? "yes" : "no";
  text += "\nPoints: " + String(p.points);
  text += "\nRaw X/Y: " + String(p.rawX) + " / " + String(p.rawY);
  text += "\nMapped X/Y: " + String(p.x) + " / " + String(p.y);
  text += "\nINT level: " + String(p.intLevel ? "HIGH" : "LOW");
  text += "\nLast touch ms: " + String(p.lastTouchMs);
  drawTextBlock(text, 28, 58, 26, TFT_WHITE);
  display_->tft().drawLine(p.x - 12, p.y, p.x + 12, p.y, TFT_YELLOW);
  display_->tft().drawLine(p.x, p.y - 12, p.x, p.y + 12, TFT_YELLOW);
  drawBackButton();
}

void UiApp::drawPaintTest() {
  display_->clear();
  display_->drawHeader("Paint test", touch_->isReady() ? "draw with finger" : "NO TOUCH");
  display_->tft().drawRect(0, 42, display_->width(), 216, TFT_DARKGREY);
  display_->drawButton(14, 270, 92, 40, "Clear", TFT_BLUE);
  drawBackButton();
}

void UiApp::drawSdTest() {
  display_->clear();
  display_->drawHeader("SD test", storage_->statusText());
  drawTextBlock(storage_->sdInfoText(), 22, 48, 20, storage_->sdAvailable() ? TFT_WHITE : TFT_RED);
  if (sdTestRun_) {
    drawTextBlock(sdTestResult_.message, 22, 142, 20, sdTestResult_.ok ? TFT_GREEN : TFT_RED);
    drawTextBlock(sdTestResult_.readBack.substring(0, 180), 22, 168, 18, TFT_LIGHTGREY);
  } else {
    drawTextBlock("Press Run to create and read:\n/BIKE_SPEEDOMETER_SD_TEST.txt", 22, 146, 20,
                  TFT_LIGHTGREY);
  }
  display_->drawButton(22, 270, 120, 40, "Run test", TFT_DARKGREEN, TFT_WHITE,
                       storage_->sdAvailable() && !storage_->usbModeActive());
  drawBackButton();
}

void UiApp::drawUsbStorage() {
  display_->clear();
  display_->drawHeader("USB Storage", usb_->active() ? "ACTIVE" : "ERROR");
  String text;
  if (usb_->active()) {
    text += "USB Storage Active\n";
    text += "SD card is exposed to computer\n";
    text += "Ride logging is disabled\n\n";
    text += "Safe eject on computer first.\n";
    text += "Reboot device after USB mode.";
  } else {
    text += usb_->status();
    text += "\n\nSD status: ";
    text += storage_->statusText();
  }
  drawTextBlock(text, 38, 66, 24, usb_->active() ? TFT_WHITE : TFT_RED);
  if (!usb_->active()) {
    drawBackButton();
  }
}

void UiApp::drawSensorTest() {
  display_->clear();
  display_->drawHeader("Sensor GPIO4 test", "GPIO4");
  String text;
  text += "Pin: GPIO";
  text += String(hw::PIN_HALL_SENSOR);
  text += "\nLevel: ";
  text += sensorSnapshot_.pinLevel == HIGH ? "HIGH" : "LOW";
  text += "\nActive level: ";
  text += app::levelToString(settings_->sensorActiveLevel);
  text += "\nInterrupt edge: ";
  text += app::interruptModeToString(settings_->sensorInterruptMode);
  text += "\nPulse count: " + String(sensorSnapshot_.pulseCount);
  text += "\nRejected pulses: " + String(sensorSnapshot_.rejectedPulseCount);
  text += "\nLast pulse ms: " + String(sensorSnapshot_.lastPulseMs);
  text += "\nInterval ms: " + String(sensorSnapshot_.lastIntervalMs);
  text += "\nSpeed km/h: " + String(speed_->currentKmh(), 1);
  drawTextBlock(text, 26, 48, 22, TFT_WHITE);
  drawBackButton();
}

void UiApp::drawBatteryTest() {
  display_->clear();
  display_->drawHeader("Battery test", "disabled");
  drawTextBlock(battery_->diagnosticText(), 32, 72, 24, TFT_WHITE);
  drawBackButton();
}

void UiApp::drawSystemInfo() {
  display_->clear();
  display_->drawHeader("System info", storage_->statusText());
  String text;
  text += "Firmware: ";
  text += app::FIRMWARE_VERSION;
  text += "\nBuild: ";
  text += __DATE__;
  text += " ";
  text += __TIME__;
  text += "\nBoard: ";
  text += app::BOARD_NAME;
  text += "\nFlash: ";
  text += flashSizeText();
  text += "\nPSRAM: ";
  text += psramText();
  text += "\nFree heap: ";
  text += String(ESP.getFreeHeap());
  text += "\nDisplay: ";
  text += app::DISPLAY_NAME;
  text += "\nTouch: ";
  text += touch_->isReady() ? "OK" : "not found";
  text += "\nBattery: disabled";
  text += "\nUSB: ";
  text += usb_->active() ? "active" : "inactive";
  drawTextBlock(text, 26, 46, 21, TFT_WHITE);
  drawBackButton();
}

void UiApp::drawSettings() {
  display_->clear();
  display_->drawHeader("Settings", storage_->statusText());
  TFT_eSPI& tft = display_->tft();
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Wheel m", 82, 60, 2);
  tft.drawString(String(settings_->wheelCircumferenceM, 3), 86, 94, 2);
  display_->drawButton(24, 76, 46, 36, "-", TFT_BLUE);
  display_->drawButton(182, 76, 46, 36, "+", TFT_BLUE);

  tft.drawString("Stop km/h", 82, 116, 2);
  tft.drawString(String(settings_->stopThresholdKmh, 1), 92, 150, 2);
  display_->drawButton(24, 132, 46, 36, "-", TFT_BLUE);
  display_->drawButton(182, 132, 46, 36, "+", TFT_BLUE);

  tft.drawString("Brightness", 82, 172, 2);
  tft.drawString(String(settings_->displayBrightnessPercent) + "%", 92, 206, 2);
  display_->drawButton(24, 188, 46, 36, "-", TFT_BLUE);
  display_->drawButton(182, 188, 46, 36, "+", TFT_BLUE);

  display_->drawButton(270, 76, 170, 38,
                       String("Edge ") + app::interruptModeToString(settings_->sensorInterruptMode),
                       TFT_PURPLE);
  display_->drawButton(270, 132, 170, 38,
                       String("Active ") + app::levelToString(settings_->sensorActiveLevel), TFT_PURPLE);
  display_->drawButton(270, 188, 170, 38,
                       settings_->sensorPullupEnabled ? "Pullup ON" : "Pullup OFF", TFT_PURPLE);
  display_->drawButton(156, 270, 110, 40, "Save", TFT_DARKGREEN);
  drawBackButton();
  if (lastMessage_.length()) {
    drawTextBlock(lastMessage_, 24, 238, 20, TFT_YELLOW);
  }
}

void UiApp::drawAbout() {
  display_->clear();
  display_->drawHeader("About", storage_->statusText());
  String text;
  text += "Bike Speedometer\n";
  text += "Firmware ";
  text += app::FIRMWARE_VERSION;
  text += "\nBoard: ";
  text += app::BOARD_NAME;
  text += "\nDisplay: ";
  text += app::DISPLAY_NAME;
  text += "\nTouch: ";
  text += app::TOUCH_NAME;
  text += "\nFlash: ";
  text += flashSizeText();
  text += "\nPSRAM: ";
  text += psramText();
  text += "\nSD: ";
  text += storage_->statusText();
  text += "\nBattery monitor: disabled";
  drawTextBlock(text, 34, 54, 23, TFT_WHITE);
  drawBackButton();
}

void UiApp::drawRide() {
  display_->clear();
  const String status = storage_->statusText() + " | " + battery_->statusText() + " | " + ride_->stateText();
  display_->drawHeader("Ride", status);
  display_->drawButton(398, 4, 76, 28, "Menu", TFT_DARKGREY);

  display_->drawButton(18, 144, 52, 40, "<", TFT_DARKGREY);
  display_->drawButton(410, 144, 52, 40, ">", TFT_DARKGREY);

  TFT_eSPI& tft = display_->tft();
  if (ridePage_ == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(String(speed_->currentKmh(), 1), 240, 118, 7);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("km/h", 240, 176, 4);
    tft.drawString(String(ride_->stats().distanceM / 1000.0f, 2) + " km  " +
                       durationText(ride_->stats().movingMs),
                   240, 220, 2);
  } else if (ridePage_ == 1) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Speed graph", 96, 50, 2);
    tft.drawRect(92, 78, 296, 150, TFT_DARKGREY);
    float maxSpeed = 10.0f;
    for (float sample : graphSamples_) {
      if (sample > maxSpeed) {
        maxSpeed = sample;
      }
    }
    int prevX = 92;
    int prevY = 228;
    for (uint8_t i = 0; i < 120; ++i) {
      const uint8_t idx = (graphWriteIndex_ + i) % 120;
      const int x = 92 + (i * 296) / 119;
      const int y = 228 - static_cast<int>((graphSamples_[idx] / maxSpeed) * 148.0f);
      if (i > 0) {
        tft.drawLine(prevX, prevY, x, y, TFT_GREEN);
      }
      prevX = x;
      prevY = y;
    }
    tft.drawString("Now " + String(speed_->currentKmh(), 1) + " km/h", 96, 236, 2);
  } else {
    String text;
    text += "Current: " + String(speed_->currentKmh(), 1) + " km/h";
    text += "\nMax: " + String(ride_->stats().maxSpeedKmh, 1) + " km/h";
    text += "\nAvg: " + String(ride_->stats().avgSpeedKmh, 1) + " km/h";
    text += "\nDistance: " + String(ride_->stats().distanceM / 1000.0f, 3) + " km";
    text += "\nMoving: " + durationText(ride_->stats().movingMs);
    text += "\nElapsed: " + durationText(ride_->stats().elapsedMs);
    text += "\nPulses: " + String(ride_->stats().pulseCount);
    drawTextBlock(text, 92, 52, 24, TFT_WHITE);
  }

  display_->drawButton(16, 266, 140, 46,
                       ride_->state() == RideState::FINISHED ? "New ride" : "Start",
                       TFT_DARKGREEN,
                       TFT_WHITE,
                       ride_->state() == RideState::IDLE || ride_->state() == RideState::FINISHED);
  display_->drawButton(170, 266, 140, 46,
                       ride_->state() == RideState::PAUSED ? "Resume" : "Pause",
                       TFT_BLUE,
                       TFT_WHITE,
                       ride_->state() == RideState::RIDING || ride_->state() == RideState::PAUSED);
  display_->drawButton(324, 266, 140, 46,
                       ride_->state() == RideState::FINISHED ? "Idle" : "Stop",
                       TFT_ORANGE,
                       TFT_BLACK,
                       ride_->state() == RideState::RIDING || ride_->state() == RideState::PAUSED ||
                           ride_->state() == RideState::FINISHED);
}

void UiApp::drawBackButton() {
  display_->drawButton(kBackX, kBackY, kBackW, kBackH, "Back", TFT_DARKGREY);
}

void UiApp::drawTextBlock(const String& text, int16_t x, int16_t y, int16_t lineHeight,
                          uint16_t color, uint16_t bg, uint8_t font) {
  TFT_eSPI& tft = display_->tft();
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, bg);
  int start = 0;
  int line = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) {
      end = text.length();
    }
    tft.drawString(text.substring(start, end), x, y + line * lineHeight, font);
    start = end + 1;
    ++line;
  }
}

bool UiApp::hit(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t bw, int16_t bh) const {
  return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

String UiApp::durationText(uint32_t ms) const {
  const uint32_t totalSeconds = ms / 1000UL;
  const uint32_t hours = totalSeconds / 3600UL;
  const uint32_t minutes = (totalSeconds / 60UL) % 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", static_cast<unsigned long>(hours),
           static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
  return String(buf);
}

void UiApp::startUsbMode() {
  if (ride_->state() == RideState::RIDING) {
    ride_->pause(millis());
    saveRecoveryIfNeeded(millis(), true);
  }
  usb_->begin(*storage_);
  dirty_ = true;
}

void UiApp::saveRecoveryIfNeeded(uint32_t nowMs, bool force) {
  if (!storage_->loggingEnabled()) {
    return;
  }
  if (!force && !ride_->needsRecoverySave(nowMs, 2000)) {
    return;
  }
  String error;
  if (storage_->saveRecovery(ride_->recoveryData(), error)) {
    ride_->markRecoverySaved(nowMs);
  }
}

void UiApp::clearRecovery() {
  String error;
  storage_->clearRecovery(error);
}

void UiApp::recordGraphSample(uint32_t nowMs) {
  if (nowMs - lastGraphSampleMs_ < 1000) {
    return;
  }
  lastGraphSampleMs_ = nowMs;
  graphSamples_[graphWriteIndex_] = speed_->currentKmh();
  graphWriteIndex_ = (graphWriteIndex_ + 1) % 120;
}
