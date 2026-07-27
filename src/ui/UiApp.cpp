#include "ui/UiApp.h"

#include "config/hardware_config.h"
#include "ui/UiTheme.h"

namespace {

constexpr int16_t kBackX = 376;
constexpr int16_t kBackY = 270;
constexpr int16_t kBackW = 92;
constexpr int16_t kBackH = 40;

constexpr int16_t kStatusBarH = ui::HEADER_H;
constexpr int16_t kControlY = ui::FOOTER_Y;
constexpr int16_t kControlH = ui::FOOTER_H;
constexpr int16_t kContentTop = ui::HEADER_H + 10;
constexpr int16_t kBottomY = ui::FOOTER_Y;
constexpr int16_t kDotsY = kBottomY - 14;
constexpr int16_t kContentBottom = kBottomY - 26;
constexpr int16_t kRideSwipeMinDx = 14;
constexpr int16_t kRideSwipeMaxDy = 140;
constexpr uint32_t kRideSwipeMaxMs = 1600;

String flashSizeText() {
  return String(static_cast<uint32_t>(ESP.getFlashChipSize() / (1024UL * 1024UL))) + " MB";
}

String psramText() {
  if (!psramFound()) {
    return "not found";
  }
  return String(static_cast<uint32_t>(ESP.getPsramSize() / (1024UL * 1024UL))) + " MB";
}

void drawBikeIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color, uint8_t scale = 1) {
  const int16_t r = 8 * scale;
  const int16_t leftX = cx - 24 * scale;
  const int16_t rightX = cx + 24 * scale;
  tft.drawCircle(leftX, cy + 10 * scale, r, color);
  tft.drawCircle(rightX, cy + 10 * scale, r, color);
  tft.drawLine(leftX, cy + 10 * scale, cx - 7 * scale, cy - 8 * scale, color);
  tft.drawLine(cx - 7 * scale, cy - 8 * scale, cx + 8 * scale, cy + 10 * scale, color);
  tft.drawLine(leftX, cy + 10 * scale, cx + 8 * scale, cy + 10 * scale, color);
  tft.drawLine(cx - 7 * scale, cy - 8 * scale, cx + 16 * scale, cy - 8 * scale, color);
  tft.drawLine(cx + 16 * scale, cy - 8 * scale, rightX, cy + 10 * scale, color);
  tft.drawLine(cx - 9 * scale, cy - 10 * scale, cx - 14 * scale, cy - 16 * scale, color);
  tft.drawLine(cx + 14 * scale, cy - 10 * scale, cx + 22 * scale, cy - 15 * scale, color);
}

void drawPulseIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawCircle(cx, cy, 19, color);
  tft.drawLine(cx - 15, cy, cx - 7, cy, color);
  tft.drawLine(cx - 7, cy, cx - 2, cy - 12, color);
  tft.drawLine(cx - 2, cy - 12, cx + 5, cy + 10, color);
  tft.drawLine(cx + 5, cy + 10, cx + 11, cy, color);
  tft.drawLine(cx + 11, cy, cx + 16, cy, color);
}

void drawGearIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawCircle(cx, cy, 17, color);
  tft.drawCircle(cx, cy, 6, color);
  tft.fillRect(cx - 3, cy - 27, 6, 8, color);
  tft.fillRect(cx - 3, cy + 19, 6, 8, color);
  tft.fillRect(cx - 27, cy - 3, 8, 6, color);
  tft.fillRect(cx + 19, cy - 3, 8, 6, color);
  tft.drawLine(cx - 18, cy - 18, cx - 24, cy - 24, color);
  tft.drawLine(cx + 18, cy - 18, cx + 24, cy - 24, color);
  tft.drawLine(cx - 18, cy + 18, cx - 24, cy + 24, color);
  tft.drawLine(cx + 18, cy + 18, cx + 24, cy + 24, color);
}

void drawUsbIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawLine(cx, cy - 23, cx, cy + 18, color);
  tft.drawLine(cx, cy - 6, cx - 18, cy + 5, color);
  tft.drawLine(cx, cy - 3, cx + 17, cy - 13, color);
  tft.fillCircle(cx, cy + 22, 4, color);
  tft.fillCircle(cx - 20, cy + 6, 4, color);
  tft.fillRect(cx + 14, cy - 17, 8, 8, color);
  tft.fillTriangle(cx - 5, cy - 23, cx + 5, cy - 23, cx, cy - 32, color);
}

void drawInfoIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawCircle(cx, cy, 20, color);
  tft.fillCircle(cx, cy - 10, 2, color);
  tft.fillRect(cx - 2, cy - 4, 4, 17, color);
}

void drawGaugeIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawCircle(cx, cy, 13, color);
  tft.fillRect(cx - 14, cy + 1, 28, 14, ui::UI_PANEL);
  tft.drawLine(cx, cy, cx + 9, cy - 6, color);
  tft.drawLine(cx - 9, cy + 1, cx - 12, cy + 5, color);
  tft.drawLine(cx + 9, cy + 1, cx + 12, cy + 5, color);
}

void drawClockIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawCircle(cx, cy, 12, color);
  tft.drawLine(cx, cy, cx, cy - 8, color);
  tft.drawLine(cx, cy, cx + 7, cy + 4, color);
}

void drawLine2(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void drawLine3(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

void drawRoadIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.fillTriangle(cx - 16, cy + 14, cx - 4, cy - 14, cx + 4, cy + 14, color);
  tft.fillTriangle(cx + 16, cy + 14, cx + 4, cy - 14, cx - 4, cy + 14, color);
  tft.drawLine(cx, cy - 8, cx, cy - 2, ui::UI_PANEL);
  tft.drawLine(cx, cy + 4, cx, cy + 10, ui::UI_PANEL);
}

void drawRideArrowIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color, uint16_t bg) {
  for (int16_t i = 0; i < 3; ++i) {
    const int16_t x = cx - 24 + i * 18;
    tft.drawWideLine(x, cy - 13, x + 12, cy, 3.0f, color, bg);
    tft.drawWideLine(x, cy + 13, x + 12, cy, 3.0f, color, bg);
  }
}

void drawMenuPulseIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  const int16_t x = cx - 18;
  drawLine2(tft, x, cy, x + 7, cy, color);
  drawLine2(tft, x + 7, cy, x + 12, cy - 12, color);
  drawLine2(tft, x + 12, cy - 12, x + 20, cy + 12, color);
  drawLine2(tft, x + 20, cy + 12, x + 28, cy - 4, color);
  drawLine2(tft, x + 28, cy - 4, x + 36, cy - 4, color);
}

void drawSlidersIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  const int16_t x = cx - 16;
  drawLine2(tft, x, cy - 10, x + 32, cy - 10, color);
  drawLine2(tft, x, cy, x + 32, cy, color);
  drawLine2(tft, x, cy + 10, x + 32, cy + 10, color);
  tft.fillCircle(cx - 6, cy - 10, 4, color);
  tft.fillCircle(cx + 8, cy, 4, color);
  tft.fillCircle(cx - 1, cy + 10, 4, color);
}

void drawStorageIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  const int16_t x = cx - 17;
  const int16_t y = cy - 13;
  tft.drawRoundRect(x, y, 34, 26, 4, color);
  tft.drawRoundRect(x + 1, y + 1, 32, 24, 3, color);
  drawLine2(tft, x + 7, y + 9, x + 27, y + 9, color);
  drawLine2(tft, x + 7, y + 17, x + 20, y + 17, color);
  tft.fillCircle(x + 27, y + 18, 3, color);
}

void drawMenuInfoIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t color) {
  tft.drawCircle(cx, cy, 17, color);
  tft.drawCircle(cx, cy, 16, color);
  tft.fillCircle(cx, cy - 8, 2, color);
  tft.fillRoundRect(cx - 2, cy - 2, 4, 14, 2, color);
}

void drawLine2(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  tft.drawLine(x0, y0, x1, y1, color);
  if (abs(x1 - x0) > abs(y1 - y0)) {
    tft.drawLine(x0, y0 + 1, x1, y1 + 1, color);
  } else {
    tft.drawLine(x0 + 1, y0, x1 + 1, y1, color);
  }
}

void drawLine3(TFT_eSPI& tft, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
  drawLine2(tft, x0, y0, x1, y1, color);
  if (abs(x1 - x0) > abs(y1 - y0)) {
    tft.drawLine(x0, y0 - 1, x1, y1 - 1, color);
  } else {
    tft.drawLine(x0 - 1, y0, x1 - 1, y1, color);
  }
}

void drawPanel(TFT_eSPI& tft, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t fill,
               uint16_t border, bool active = false) {
  tft.fillRoundRect(x, y, w, h, ui::RADIUS, fill);
  const uint8_t strokeWidth = active ? 2 : 1;
  for (uint8_t i = 0; i < strokeWidth; ++i) {
    tft.drawRoundRect(x + i, y + i, w - i * 2, h - i * 2, ui::RADIUS, border);
  }
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

  if (screen_ == Screen::Ride) {
    if (touchedNow && !wasTouched_) {
      handleRideTouchStart(touch_->x(), touch_->y(), now);
    } else if (touchedNow && wasTouched_) {
      handleRideTouchMove(touch_->x(), touch_->y());
    } else if (!touchedNow && wasTouched_) {
      handleRideTouchEnd(now);
    }
  } else if (touchedNow && !wasTouched_) {
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
  rideTouchActive_ = false;
  rideSwipeCandidate_ = false;
  rideSwipeHandled_ = false;
  if (screen == Screen::SdTest) {
    sdTestRun_ = false;
    sdTestResult_ = SdTestResult();
  }
}

void UiApp::updateModel(uint32_t nowMs) {
  sensorSnapshot_ = sensor_->snapshot();
  speed_->update(sensorSnapshot_, *settings_, nowMs);
  ride_->update(nowMs, speed_->currentKmh(), sensorSnapshot_.pulseCount);
  battery_->update(nowMs);
  recordGraphSample(nowMs);
  saveRecoveryIfNeeded(nowMs, false);
}

void UiApp::handleRideTouchStart(int16_t x, int16_t y, uint32_t nowMs) {
  rideTouchActive_ = true;
  rideTouchStartX_ = x;
  rideTouchStartY_ = y;
  rideTouchLastX_ = x;
  rideTouchLastY_ = y;
  rideTouchStartMs_ = nowMs;
  rideSwipeCandidate_ = y >= kStatusBarH && y < kControlY;
  rideSwipeHandled_ = false;
}

void UiApp::handleRideTouchMove(int16_t x, int16_t y) {
  if (!rideTouchActive_) {
    return;
  }
  rideTouchLastX_ = x;
  rideTouchLastY_ = y;
  if (!rideSwipeCandidate_ || rideSwipeHandled_) {
    return;
  }

  const int16_t dx = rideTouchLastX_ - rideTouchStartX_;
  const int16_t dy = rideTouchLastY_ - rideTouchStartY_;
  const bool horizontalSwipe = abs(dx) >= kRideSwipeMinDx && abs(dy) <= kRideSwipeMaxDy &&
                               abs(dx) * 5 > abs(dy);
  if (!horizontalSwipe) {
    return;
  }

  if (dx < 0) {
    ridePage_ = (ridePage_ + 1) % 3;
  } else {
    ridePage_ = ridePage_ == 0 ? 2 : ridePage_ - 1;
  }
  rideSwipeHandled_ = true;
  dirty_ = true;
}

void UiApp::handleRideTouchEnd(uint32_t nowMs) {
  if (!rideTouchActive_) {
    return;
  }

  const int16_t dx = rideTouchLastX_ - rideTouchStartX_;
  const int16_t dy = rideTouchLastY_ - rideTouchStartY_;
  const uint32_t elapsedMs = nowMs - rideTouchStartMs_;
  const bool horizontalSwipe = !rideSwipeHandled_ && abs(dx) >= kRideSwipeMinDx && abs(dy) <= kRideSwipeMaxDy &&
                               elapsedMs <= kRideSwipeMaxMs && abs(dx) > abs(dy) * 2;

  if (rideSwipeHandled_) {
    // Swipe was already applied while the finger was moving.
  } else if (rideSwipeCandidate_ && horizontalSwipe) {
    if (dx < 0) {
      ridePage_ = (ridePage_ + 1) % 3;
    } else {
      ridePage_ = ridePage_ == 0 ? 2 : ridePage_ - 1;
    }
    dirty_ = true;
  } else {
    handleTap(rideTouchLastX_, rideTouchLastY_);
  }

  rideTouchActive_ = false;
  rideSwipeCandidate_ = false;
  rideSwipeHandled_ = false;
}

void UiApp::handleTap(int16_t x, int16_t y) {
  switch (screen_) {
    case Screen::SdMissing:
      if (hit(x, y, 70, 232, 150, 46)) {
        if (storage_->retry()) {
          String error;
          storage_->loadSettings(*settings_, error);
          display_->setBrightness(settings_->displayBrightnessPercent);
          enter(Screen::MainMenu);
        } else {
          lastMessage_ = "SD retry failed";
          dirty_ = true;
        }
      } else if (hit(x, y, 250, 232, 170, 46)) {
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
      if (hit(x, y, 24, 70, 132, 82)) {
        enter(Screen::Ride);
      } else if (hit(x, y, 174, 70, 132, 82)) {
        enter(Screen::Diagnostics);
      } else if (hit(x, y, 324, 70, 132, 82)) {
        enter(Screen::Settings);
      } else if (hit(x, y, 24, 170, 132, 82) && storage_->sdAvailable()) {
        startUsbMode();
        enter(Screen::UsbStorage);
      } else if (hit(x, y, 174, 170, 132, 82)) {
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
      if (hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        if (usb_->active()) {
          usb_->end(*storage_);
        }
        enter(Screen::MainMenu);
      }
      break;

    case Screen::Settings:
      if (hit(x, y, 252, 48, 42, 32)) {
        settings_->wheelCircumferenceM -= 0.005f;
      } else if (hit(x, y, 416, 48, 42, 32)) {
        settings_->wheelCircumferenceM += 0.005f;
      } else if (hit(x, y, 252, 84, 42, 32)) {
        settings_->stopThresholdKmh -= 0.5f;
      } else if (hit(x, y, 416, 84, 42, 32)) {
        settings_->stopThresholdKmh += 0.5f;
      } else if (hit(x, y, 284, 120, 174, 32)) {
        settings_->sensorInterruptMode =
            settings_->sensorInterruptMode == FALLING ? RISING
                                                      : (settings_->sensorInterruptMode == RISING ? CHANGE : FALLING);
      } else if (hit(x, y, 284, 156, 174, 32)) {
        settings_->sensorActiveLevel = settings_->sensorActiveLevel == LOW ? HIGH : LOW;
      } else if (hit(x, y, 284, 192, 174, 32)) {
        settings_->sensorPullupEnabled = !settings_->sensorPullupEnabled;
      } else if (hit(x, y, 14, 270, 110, 40)) {
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
      } else if (hit(x, y, 0, kControlY, 160, kControlH)) {
        if (ride_->state() == RideState::IDLE || ride_->state() == RideState::FINISHED) {
          ride_->start(millis(), sensorSnapshot_.pulseCount);
          saveRecoveryIfNeeded(millis(), true);
        }
        dirty_ = true;
      } else if (hit(x, y, 160, kControlY, 160, kControlH)) {
        if (ride_->state() == RideState::RIDING) {
          ride_->pause(millis());
        } else if (ride_->state() == RideState::PAUSED) {
          ride_->resume(millis());
        }
        saveRecoveryIfNeeded(millis(), true);
        dirty_ = true;
      } else if (hit(x, y, 320, kControlY, 160, kControlH)) {
        if (ride_->state() == RideState::RIDING || ride_->state() == RideState::PAUSED) {
          const uint32_t nowMs = millis();
          ride_->update(nowMs, speed_->currentKmh(), sensorSnapshot_.pulseCount);
          ride_->finish(nowMs);
          clearRecovery();
          ride_->newRide(nowMs, sensorSnapshot_.pulseCount);
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
    tft.drawLine(lastPaintX_, lastPaintY_, x, y, ui::UI_CYAN);
  } else {
    tft.fillCircle(x, y, 2, ui::UI_CYAN);
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

void UiApp::drawStatusBar(const String& title, const String& status) {
  TFT_eSPI& tft = display_->tft();
  tft.fillRect(0, 0, display_->width(), kStatusBarH, ui::UI_BG);
  tft.drawFastHLine(ui::SAFE, kStatusBarH - 1, display_->width() - ui::SAFE * 2, ui::BORDER_SOFT);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(ui::UI_TEXT, ui::UI_BG);
  tft.drawString(title, ui::SAFE, kStatusBarH / 2, 2);
  if (status.length() > 0) {
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(ui::UI_MUTED, ui::UI_BG);
    tft.drawString(status, display_->width() - ui::SAFE, kStatusBarH / 2, 2);
  }
}

void UiApp::drawStorageStatusIcon(int16_t x, int16_t y) {
  TFT_eSPI& tft = display_->tft();
  const bool available = storage_->sdAvailable();
  const uint16_t color = available ? ui::UI_TEXT : ui::UI_RED;
  const int16_t w = 19;
  const int16_t h = 23;
  const int16_t left = x - w;
  const int16_t top = y - h / 2;

  if (available) {
    tft.fillRect(left + 1, top + 6, w - 2, h - 7, color);
    tft.fillTriangle(left + 1, top + 6, left + 7, top, left + 7, top + 6, color);
    tft.fillRect(left + 7, top, w - 8, 7, color);
    tft.drawRect(left + 5, top + 3, 4, 4, ui::UI_BG);
    tft.drawRect(left + 10, top + 3, 4, 4, ui::UI_BG);
    tft.drawFastHLine(left + 5, top + 16, 9, ui::UI_BG);
    return;
  }

  tft.drawLine(left + 1, top + 6, left + 7, top, color);
  tft.drawLine(left + 7, top, left + w - 2, top, color);
  tft.drawLine(left + w - 1, top + 1, left + w - 1, top + h - 2, color);
  tft.drawLine(left + w - 2, top + h - 1, left + 1, top + h - 1, color);
  tft.drawLine(left, top + h - 2, left, top + 7, color);
  tft.drawLine(left + 4, top + 15, left + 14, top + 15, color);
  tft.drawLine(left + 4, top + 18, left + 11, top + 18, color);
}

void UiApp::drawBatteryStatusIcon(int16_t x, int16_t y) {
  TFT_eSPI& tft = display_->tft();
  const bool measured = battery_->enabled();
  const uint16_t color = measured ? ui::UI_TEXT : ui::UI_RED;
  const uint16_t bg = ui::UI_BG;
  const int16_t bodyW = 26;
  const int16_t bodyH = 13;
  const int16_t bodyX = x - bodyW - 4;
  const int16_t bodyY = y - bodyH / 2;

  if (measured) {
    tft.drawRoundRect(bodyX, bodyY, bodyW, bodyH, 2, color);
    tft.fillRect(bodyX + bodyW, bodyY + 4, 3, 5, color);
    tft.fillRect(bodyX + 3, bodyY + 3, bodyW - 6, bodyH - 6, color);
    return;
  }

  tft.drawRoundRect(bodyX, bodyY, bodyW, bodyH, 2, color);
  tft.drawRect(bodyX + bodyW, bodyY + 4, 3, 5, color);
  tft.drawLine(bodyX + 13, bodyY + 1, bodyX + 9, bodyY + 6, color);
  tft.drawLine(bodyX + 9, bodyY + 6, bodyX + 14, bodyY + 6, color);
  tft.drawLine(bodyX + 14, bodyY + 6, bodyX + 10, bodyY + 12, color);
  tft.drawLine(bodyX + 14, bodyY + 1, bodyX + 10, bodyY + 6, color);
  tft.drawLine(bodyX + 10, bodyY + 6, bodyX + 15, bodyY + 6, color);
  tft.drawLine(bodyX + 15, bodyY + 6, bodyX + 11, bodyY + 12, color);
}

void UiApp::drawSoftButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                           uint16_t accentColor, bool enabled, uint16_t fillColor) {
  TFT_eSPI& tft = display_->tft();
  const uint16_t bg = fillColor == 0xFFFF ? ui::UI_PANEL : fillColor;
  const uint16_t fg = enabled ? accentColor : ui::UI_DISABLED;
  const bool active = enabled && accentColor == ui::UI_CYAN && fillColor == ui::UI_BG;
  const uint16_t border = enabled ? (active ? ui::BORDER_ACTIVE : ui::BORDER) : ui::DISABLED_COLOR;
  drawPanel(tft, x, y, w, h, bg, border, active);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
}

void UiApp::drawMenuTile(int16_t x, int16_t y, int16_t w, int16_t h, const String& title,
                         bool selected, bool enabled) {
  TFT_eSPI& tft = display_->tft();
  const uint16_t border = !enabled ? ui::UI_DISABLED : (selected ? ui::BORDER_ACTIVE : ui::BORDER_SOFT);
  const uint16_t fg = !enabled ? ui::UI_DISABLED : (selected ? ui::UI_CYAN : ui::UI_TEXT);
  drawPanel(tft, x, y, w, h, selected && enabled ? ui::UI_PANEL_ACTIVE : ui::UI_PANEL, border,
            selected && enabled);

  const int16_t cx = x + w / 2;
  const int16_t iconY = y + 30;
  const uint16_t icon = enabled ? ui::LABEL : ui::UI_DISABLED;
  const uint16_t tileFill = selected && enabled ? ui::UI_PANEL_ACTIVE : ui::UI_PANEL;
  if (title == "Ride") {
    drawRideArrowIcon(tft, cx, iconY, icon, tileFill);
  } else if (title == "Diagnostics") {
    drawMenuPulseIcon(tft, cx, iconY, icon);
  } else if (title == "Settings") {
    drawSlidersIcon(tft, cx, iconY, icon);
  } else if (title == "Storage" || title == "USB Storage") {
    drawStorageIcon(tft, cx, iconY, icon);
  } else if (title == "About") {
    drawMenuInfoIcon(tft, cx, iconY, icon);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, tileFill);
  String label = title;
  label.toUpperCase();
  tft.drawString(label, cx, y + h - 20, 2);
}

void UiApp::drawMetricCard(int16_t x, int16_t y, int16_t w, int16_t h, const String& label,
                           const String& value, const String& unit) {
  TFT_eSPI& tft = display_->tft();
  drawPanel(tft, x, y, w, h, ui::UI_PANEL, ui::UI_LINE_SOFT);
  const int16_t ix = x + 24;
  const int16_t iy = y + 24;
  if (label == "MAX SPEED" || label == "AVG SPEED") {
    drawGaugeIcon(tft, ix, iy, ui::TEXT_MUTED);
  } else if (label == "DISTANCE") {
    drawRoadIcon(tft, ix, iy, ui::TEXT_MUTED);
  } else if (label == "MOVING" || label == "ELAPSED") {
    drawClockIcon(tft, ix, iy, ui::TEXT_MUTED);
  } else {
    tft.fillCircle(ix, iy, 10, ui::TEXT_MUTED);
  }

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(ui::UI_CYAN, ui::UI_PANEL);
  tft.drawString(label, x + 46, y + 17, 2);

  const bool hasUnit = unit.length() > 0;
  const bool longValue = value.length() > 5;
  const uint8_t valueFont = longValue ? 2 : 4;
  const int16_t valueY = hasUnit ? y + 57 : (longValue ? y + 60 : y + 58);

  tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
  if (unit.length() > 0) {
    const int16_t valueW = tft.textWidth(value, valueFont);
    const int16_t unitW = tft.textWidth(unit, 2);
    const int16_t gap = 6;
    const int16_t groupW = valueW + gap + unitW;
    const int16_t groupX = x + (w - groupW) / 2;

    tft.setTextDatum(ML_DATUM);
    tft.drawString(value, groupX, valueY, valueFont);
    tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
    tft.drawString(unit, groupX + valueW + gap, valueY + 3, 2);
  } else {
    tft.setTextDatum(MC_DATUM);
    tft.drawString(value, x + w / 2, valueY, valueFont);
  }
}

void UiApp::drawPageDots(uint8_t activePage, uint8_t pageCount) {
  TFT_eSPI& tft = display_->tft();
  const int16_t spacing = 18;
  const int16_t startX = 240 - ((pageCount - 1) * spacing) / 2;
  for (uint8_t i = 0; i < pageCount; ++i) {
    tft.fillCircle(startX + i * spacing, kDotsY, i == activePage ? 5 : 4,
                   i == activePage ? ui::UI_CYAN : ui::UI_LINE);
  }
}

void UiApp::drawGraphCard(int16_t x, int16_t y, int16_t w, int16_t h) {
  TFT_eSPI& tft = display_->tft();
  drawPanel(tft, x, y, w, h, ui::UI_PANEL, ui::UI_LINE_SOFT);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ui::UI_CYAN, ui::UI_PANEL);
  tft.drawString("SPEED", x + 16, y + 16, 2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
  tft.drawString(String(speed_->currentKmh(), 1) + " km/h", x + w - 16, y + 16, 4);

  const int16_t gx = x + 52;
  const int16_t gy = y + 52;
  const int16_t gw = w - 78;
  const int16_t gh = h - 84;
  const float maxSpeed = 40.0f;

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
  for (uint8_t i = 0; i < 5; ++i) {
    const int16_t lineY = gy + gh - (i * gh) / 4;
    tft.drawFastHLine(gx, lineY, gw, ui::UI_GRID);
    tft.drawString(String(i * 10), gx - 10, lineY, 2);
  }
  tft.setTextDatum(TL_DATUM);
  tft.drawString("-120s", gx, gy + gh + 12, 2);
  tft.setTextDatum(TR_DATUM);
  tft.drawString("now", gx + gw, gy + gh + 12, 2);
  tft.drawFastVLine(gx + gw, gy, gh, ui::UI_MUTED);

  int16_t prevX = gx;
  int16_t prevY = gy + gh;
  for (uint8_t i = 0; i < 120; ++i) {
    const uint8_t idx = (graphWriteIndex_ + i) % 120;
    const float sample = constrain(graphSamples_[idx], 0.0f, maxSpeed);
    const int16_t px = gx + (i * gw) / 119;
    const int16_t py = gy + gh - static_cast<int16_t>((sample / maxSpeed) * gh);
    if (i > 0) {
      drawLine3(tft, prevX, prevY, px, py, ui::UI_CYAN);
    }
    prevX = px;
    prevY = py;
  }
  tft.fillCircle(prevX, prevY, 5, ui::UI_CYAN);
}

void UiApp::drawBottomRideControls() {
  const RideState state = ride_->state();
  TFT_eSPI& tft = display_->tft();
  tft.fillRect(0, kControlY, display_->width(), kControlH, ui::UI_BG);
  tft.drawFastHLine(ui::SAFE, kControlY, display_->width() - ui::SAFE * 2, ui::BORDER_SOFT);
  tft.drawFastVLine(160, kControlY + 10, kControlH - 20, ui::BORDER_SOFT);
  tft.drawFastVLine(320, kControlY + 10, kControlH - 20, ui::BORDER_SOFT);

  auto drawSegment = [&](int16_t x, int16_t w, const String& label, uint8_t iconKind,
                         uint16_t color, bool enabled) {
    const uint16_t fg = enabled ? color : ui::UI_DISABLED;
    constexpr int16_t iconW = 28;
    constexpr int16_t gap = 12;
    const int16_t textW = tft.textWidth(label, 4);
    const int16_t groupW = iconW + gap + textW;
    const int16_t iconX = x + (w - groupW) / 2;
    const int16_t cy = kControlY + 32;
    if (iconKind == 0) {
      tft.fillTriangle(iconX, cy - 14, iconX, cy + 14, iconX + 26, cy, fg);
    } else if (iconKind == 1) {
      tft.fillRoundRect(iconX + 3, cy - 15, 8, 30, 2, fg);
      tft.fillRoundRect(iconX + 17, cy - 15, 8, 30, 2, fg);
    } else {
      tft.fillRoundRect(iconX + 2, cy - 13, 26, 26, 2, fg);
    }
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(fg, ui::UI_BG);
    tft.drawString(label, iconX + iconW + gap, cy, 4);
  };

  drawSegment(0, 160, state == RideState::FINISHED ? "NEW" : "START", 0,
              ui::UI_GREEN, state == RideState::IDLE || state == RideState::FINISHED);
  drawSegment(160, 160, state == RideState::PAUSED ? "RESUME" : "PAUSE", 1,
              ui::TEXT_MUTED, state == RideState::RIDING || state == RideState::PAUSED);
  drawSegment(320, 160, "STOP", 2, ui::UI_RED,
              state == RideState::RIDING || state == RideState::PAUSED);
}

String UiApp::storageStatusShort() const {
  if (storage_->usbModeActive()) {
    return "USB";
  }
  return storage_->sdAvailable() ? "SD OK" : "NO SD";
}

String UiApp::batteryStatusShort() const {
  return battery_->enabled() ? battery_->statusText() : "BAT N/A";
}

String UiApp::rideStatusLine() const {
  return ride_->stateText();
}

void UiApp::drawSdMissing() {
  display_->clear(ui::UI_BG);
  drawStatusBar("SD card not found", "NO SD");
  TFT_eSPI& tft = display_->tft();
  drawPanel(tft, 46, 76, 388, 136, ui::UI_PANEL, ui::BORDER_SOFT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ui::UI_ORANGE, ui::UI_PANEL);
  tft.drawString("SD card not found", 240, 96, 4);
  tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
  tft.drawString("Ride logging disabled.", 240, 136, 2);
  tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
  tft.drawString("Retry SD init or continue without saving.", 240, 162, 2);
  if (lastMessage_.length()) {
    tft.setTextColor(ui::UI_RED, ui::UI_PANEL);
    tft.drawString(lastMessage_, 240, 188, 2);
  }
  drawSoftButton(70, 232, 150, 46, "RETRY", ui::UI_CYAN);
  drawSoftButton(250, 232, 170, 46, "CONTINUE", ui::UI_GREEN);
}

void UiApp::drawRecovery() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Recovery");
  drawStorageStatusIcon(452, 17);
  TFT_eSPI& tft = display_->tft();
  drawPanel(tft, 44, 58, 392, 132, ui::UI_PANEL, ui::BORDER_SOFT);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ui::UI_ORANGE, ui::UI_PANEL);
  tft.drawString("Unfinished ride found", 240, 78, 4);
  tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
  tft.drawString("Recovered as paused", 240, 116, 2);
  tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
  tft.drawString("Distance " + String(ride_->stats().distanceM / 1000.0f, 2) + " km", 240, 144, 2);
  tft.drawString("Moving " + durationText(ride_->stats().movingMs), 240, 168, 2);
  drawSoftButton(52, 204, 110, 48, "RESUME", ui::UI_GREEN);
  drawSoftButton(185, 204, 110, 48, "FINISH", ui::UI_ORANGE);
  drawSoftButton(318, 204, 110, 48, "DISCARD", ui::UI_RED);
}

void UiApp::drawMainMenu() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Menu");
  drawStorageStatusIcon(416, 17);
  drawBatteryStatusIcon(452, 17);
  drawMenuTile(24, 70, 132, 82, "Ride", true, true);
  drawMenuTile(174, 70, 132, 82, "Diagnostics", false, true);
  drawMenuTile(324, 70, 132, 82, "Settings", false, true);
  drawMenuTile(24, 170, 132, 82, "Storage", false, storage_->sdAvailable());
  drawMenuTile(174, 170, 132, 82, "About", false, true);
}

void UiApp::drawDiagnostics() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Diagnostics");
  drawStorageStatusIcon(452, 17);
  drawSoftButton(18, 48, 210, 40, "Display test", ui::UI_CYAN);
  drawSoftButton(252, 48, 210, 40, "Touch raw test", ui::UI_CYAN);
  drawSoftButton(18, 100, 210, 40, "Paint test", ui::UI_GREEN);
  drawSoftButton(252, 100, 210, 40, "SD test", ui::UI_GREEN, storage_->sdAvailable());
  drawSoftButton(18, 152, 210, 40, "USB MSC test", ui::UI_ORANGE, storage_->sdAvailable());
  drawSoftButton(252, 152, 210, 40, "Sensor test", ui::UI_CYAN);
  drawSoftButton(18, 204, 210, 40, "Battery test", ui::UI_MUTED);
  drawSoftButton(252, 204, 210, 40, "System info", ui::UI_MUTED);
  drawBackButton();
}

void UiApp::drawDisplayTest() {
  TFT_eSPI& tft = display_->tft();
  tft.fillScreen(ui::UI_BG);
  drawStatusBar("Display test", "ST7796");
  const uint16_t colors[] = {ui::UI_BG, ui::UI_TEXT, ui::UI_RED, ui::UI_GREEN, TFT_BLUE, ui::UI_CYAN};
  for (uint8_t i = 0; i < 6; ++i) {
    tft.fillRect(20 + i * 74, 54, 64, 48, colors[i]);
    tft.drawRect(20 + i * 74, 54, 64, 48, ui::UI_LINE);
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ui::UI_TEXT, ui::UI_BG);
  tft.drawString("Text readability 1234567890", 22, 122, 2);
  tft.drawString("Orientation: landscape 480x320", 22, 148, 2);
  for (int i = 0; i < 8; ++i) {
    tft.drawLine(24, 190 + i * 6, 260 + i * 18, 246, ui::UI_CYAN);
  }
  tft.drawRect(300, 130, 120, 74, ui::UI_GREEN);
  tft.drawRoundRect(316, 150, 120, 74, 10, ui::UI_RED);
  drawBackButton();
}

void UiApp::drawTouchRawTest() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Touch raw test", touch_->isReady() ? "FT6336 OK" : "NO TOUCH");
  const TouchPoint& p = touch_->point();
  String text;
  text += "Touched: ";
  text += p.touched ? "yes" : "no";
  text += "\nPoints: " + String(p.points);
  text += "\nRaw X/Y: " + String(p.rawX) + " / " + String(p.rawY);
  text += "\nMapped X/Y: " + String(p.x) + " / " + String(p.y);
  text += "\nINT level: " + String(p.intLevel ? "HIGH" : "LOW");
  text += "\nLast touch ms: " + String(p.lastTouchMs);
  drawTextBlock(text, 28, 58, 26, ui::UI_TEXT, ui::UI_BG);
  display_->tft().drawLine(p.x - 12, p.y, p.x + 12, p.y, ui::UI_CYAN);
  display_->tft().drawLine(p.x, p.y - 12, p.x, p.y + 12, ui::UI_CYAN);
  drawBackButton();
}

void UiApp::drawPaintTest() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Paint test", touch_->isReady() ? "DRAW" : "NO TOUCH");
  display_->tft().drawRect(0, 42, display_->width(), 216, ui::UI_LINE);
  drawSoftButton(14, 270, 92, 40, "CLEAR", ui::UI_CYAN);
  drawBackButton();
}

void UiApp::drawSdTest() {
  display_->clear(ui::UI_BG);
  drawStatusBar("SD test");
  drawStorageStatusIcon(452, 17);
  drawTextBlock(storage_->sdInfoText(), 22, 48, 20, storage_->sdAvailable() ? ui::UI_TEXT : ui::UI_RED,
                ui::UI_BG);
  if (sdTestRun_) {
    drawTextBlock(sdTestResult_.message, 22, 142, 20, sdTestResult_.ok ? ui::UI_GREEN : ui::UI_RED,
                  ui::UI_BG);
    drawTextBlock(sdTestResult_.readBack.substring(0, 180), 22, 168, 18, ui::UI_MUTED, ui::UI_BG);
  } else {
    drawTextBlock("Press Run to create and read:\n/BIKE_SPEEDOMETER_SD_TEST.txt", 22, 146, 20,
                  ui::UI_MUTED, ui::UI_BG);
  }
  drawSoftButton(22, 270, 120, 40, "RUN TEST", ui::UI_GREEN,
                 storage_->sdAvailable() && !storage_->usbModeActive());
  drawBackButton();
}

void UiApp::drawUsbStorage() {
  display_->clear(ui::UI_BG);
  drawStatusBar("USB Storage", usb_->active() ? "ACTIVE" : "ERROR");
  TFT_eSPI& tft = display_->tft();
  tft.setTextDatum(TC_DATUM);
  drawPanel(tft, 44, 72, 392, 146, ui::UI_PANEL, ui::BORDER_SOFT);
  if (usb_->active()) {
    tft.setTextColor(ui::UI_CYAN, ui::UI_PANEL);
    tft.drawString("USB Storage Active", 240, 96, 4);
    tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
    tft.drawString("SD card is exposed to computer.", 240, 142, 2);
    tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
    tft.drawString("Safe eject on computer first.", 240, 170, 2);
    tft.drawString("Then press EXIT to return.", 240, 194, 2);
  } else {
    tft.setTextColor(ui::UI_RED, ui::UI_PANEL);
    tft.drawString("USB Storage Error", 240, 96, 4);
    tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
    tft.drawString(usb_->status(), 240, 142, 2);
    tft.drawString("SD: " + storageStatusShort(), 240, 170, 2);
  }
  drawSoftButton(kBackX, kBackY, kBackW, kBackH, usb_->active() ? "EXIT" : "BACK", ui::UI_CYAN);
}

void UiApp::drawSensorTest() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Sensor test", "GPIO4");
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
  drawTextBlock(text, 26, 48, 22, ui::UI_TEXT, ui::UI_BG);
  drawBackButton();
}

void UiApp::drawBatteryTest() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Battery test", battery_->enabled() ? battery_->stateText() : "BAT N/A");
  drawBatteryStatusIcon(452, 17);
  drawTextBlock(battery_->diagnosticText(), 32, 72, 24, ui::UI_MUTED, ui::UI_BG);
  drawBackButton();
}

void UiApp::drawSystemInfo() {
  display_->clear(ui::UI_BG);
  drawStatusBar("System info");
  drawStorageStatusIcon(452, 17);
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
  drawTextBlock(text, 26, 46, 21, ui::UI_TEXT, ui::UI_BG);
  drawBackButton();
}

void UiApp::drawSettings() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Settings", lastMessage_);
  if (!lastMessage_.length()) {
    drawStorageStatusIcon(452, 17);
  }
  TFT_eSPI& tft = display_->tft();
  auto drawRowBase = [&](int16_t y, const String& label) {
    tft.fillRoundRect(ui::SAFE, y, ui::SCREEN_W - ui::SAFE * 2, 34, 6, ui::SURFACE);
    tft.drawFastHLine(ui::SAFE + 8, y + 33, ui::SCREEN_W - ui::SAFE * 2 - 16, ui::BORDER_SOFT);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ui::TEXT, ui::SURFACE);
    tft.drawString(label, ui::SAFE + 12, y + 17, 2);
  };
  auto drawStepper = [&](int16_t x, int16_t y, const String& label) {
    tft.fillRoundRect(x, y, 42, 28, 6, ui::SURFACE_2);
    tft.drawRoundRect(x, y, 42, 28, 6, ui::BORDER);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ui::LABEL, ui::SURFACE_2);
    tft.drawString(label, x + 21, y + 14, 2);
  };
  auto drawValue = [&](int16_t y, const String& value) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ui::TEXT, ui::SURFACE);
    tft.drawString(value, 356, y + 17, 2);
  };
  auto drawPill = [&](int16_t y, const String& value) {
    tft.fillRoundRect(284, y + 3, 174, 28, 6, ui::SURFACE_2);
    tft.drawRoundRect(284, y + 3, 174, 28, 6, ui::BORDER);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ui::LABEL, ui::SURFACE_2);
    tft.drawString(value, 371, y + 17, 2);
  };

  drawRowBase(48, "Wheel circumference");
  drawStepper(252, 51, "-");
  drawValue(48, String(settings_->wheelCircumferenceM, 3) + " m");
  drawStepper(416, 51, "+");

  drawRowBase(84, "Stop threshold");
  drawStepper(252, 87, "-");
  drawValue(84, String(settings_->stopThresholdKmh, 1) + " km/h");
  drawStepper(416, 87, "+");

  drawRowBase(120, "Sensor edge");
  drawPill(120, app::interruptModeToString(settings_->sensorInterruptMode));
  drawRowBase(156, "Active level");
  drawPill(156, app::levelToString(settings_->sensorActiveLevel));
  drawRowBase(192, "Pullup");
  drawPill(192, settings_->sensorPullupEnabled ? "ON" : "OFF");

  drawSoftButton(14, 270, 110, 40, "SAVE", ui::UI_GREEN);
  drawBackButton();
}

void UiApp::drawAbout() {
  display_->clear(ui::UI_BG);
  drawStatusBar("About");
  drawStorageStatusIcon(452, 17);
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
  drawTextBlock(text, 34, 54, 23, ui::UI_TEXT, ui::UI_BG);
  drawBackButton();
}

void UiApp::drawRide() {
  display_->clear(ui::UI_BG);
  drawStatusBar("Ride");
  TFT_eSPI& tft = display_->tft();
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(ui::UI_MUTED, ui::UI_BG);
  tft.drawString(rideStatusLine(), 292, 17, 2);
  drawStorageStatusIcon(326, 17);
  drawBatteryStatusIcon(366, 17);
  drawSoftButton(398, 4, 76, 26, "MENU", ui::UI_CYAN, true, ui::UI_BG);
  if (ridePage_ == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ui::UI_CYAN, ui::UI_BG);
    tft.drawString("SPEED", 240, 58, 2);
    tft.setTextColor(ui::UI_TEXT, ui::UI_BG);
    tft.drawString(String(speed_->currentKmh(), 1), 240, 118, 8);
    tft.setTextColor(ui::UI_CYAN, ui::UI_BG);
    tft.drawString("km/h", 240, 166, 2);

    drawPanel(tft, 24, 178, 208, 44, ui::UI_PANEL, ui::UI_LINE_SOFT);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ui::UI_CYAN, ui::UI_PANEL);
    tft.drawString("DISTANCE", 40, 190, 2);
    tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
    tft.drawString(String(ride_->stats().distanceM / 1000.0f, 2), 134, 204, 4);
    tft.setTextColor(ui::UI_MUTED, ui::UI_PANEL);
    tft.drawString("km", 198, 206, 2);

    drawPanel(tft, 248, 178, 208, 44, ui::UI_PANEL, ui::UI_LINE_SOFT);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ui::UI_CYAN, ui::UI_PANEL);
    tft.drawString("TIME", 264, 190, 2);
    tft.setTextColor(ui::UI_TEXT, ui::UI_PANEL);
    tft.drawString(durationText(ride_->stats().movingMs), 328, 206, 2);
  } else if (ridePage_ == 1) {
    drawGraphCard(20, 58, 440, 160);
  } else {
    drawMetricCard(20, 46, 140, 86, "MAX SPEED", String(ride_->stats().maxSpeedKmh, 1), "km/h");
    drawMetricCard(170, 46, 140, 86, "AVG SPEED", String(ride_->stats().avgSpeedKmh, 1), "km/h");
    drawMetricCard(320, 46, 140, 86, "DISTANCE",
                   String(ride_->stats().distanceM / 1000.0f, 2), "km");
    drawMetricCard(20, 138, 140, 86, "MOVING", durationText(ride_->stats().movingMs));
    drawMetricCard(170, 138, 140, 86, "ELAPSED", durationText(ride_->stats().elapsedMs));
    drawMetricCard(320, 138, 140, 86, "PULSES", String(ride_->stats().pulseCount));
  }
  drawPageDots(ridePage_, 3);
  drawBottomRideControls();
}

void UiApp::drawBackButton() {
  drawSoftButton(kBackX, kBackY, kBackW, kBackH, "BACK", ui::UI_CYAN);
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
