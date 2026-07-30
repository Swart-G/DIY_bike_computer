#include "ui/UiApp.h"

#include "config/hardware_config.h"
#include "ui/UiTheme.h"
#include "ui/components/UiComponents.h"
#include "ui/screens/HomeScreen.h"
#include "ui/screens/RideScreen.h"
#include "ui/screens/RainLockOverlay.h"
#include "ui/screens/SecondaryScreens.h"
#include "ui/screens/SettingsScreen.h"
#include "ui_exact/exact_screen_renderer.h"

namespace {

constexpr int16_t kBackX = 376;
constexpr int16_t kBackY = 270;
constexpr int16_t kBackW = 92;
constexpr int16_t kBackH = 40;

constexpr int16_t kStatusBarH = ui::HEADER_H;
constexpr int16_t kControlY = ui::FOOTER_Y;
constexpr int16_t kRideSwipeMinDx = 64;
constexpr int16_t kRideSwipeMaxDy = 48;
constexpr uint32_t kRideSwipeMaxMs = 700;
constexpr int16_t kHistorySwipeMinDy = 46;
constexpr int16_t kHistorySwipeMaxDx = 64;
constexpr uint32_t kHistorySwipeMaxMs = 800;

bool headerBackHit(int16_t x, int16_t y) {
  // The drawn chevron is intentionally compact, but the touch target also
  // covers the title-side padding so it remains easy to hit while riding.
  return x >= 0 && x < 126 && y >= 0 && y < 58;
}

String flashSizeText() {
  return String(static_cast<uint32_t>(ESP.getFlashChipSize() / (1024UL * 1024UL))) + " MB";
}

String psramText() {
  if (!psramFound()) {
    return "not found";
  }
  return String(static_cast<uint32_t>(ESP.getPsramSize() / (1024UL * 1024UL))) + " MB";
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
                  SpeedTrendLed& speedTrend,
                  RideStateMachine& ride, BatteryMonitor& battery, RideLogger& logger,
                  RideRepository& repository, PhoneLinkManager& phone,
                  app::AppSettings& settings) {
  display_ = &display;
  touch_ = &touch;
  storage_ = &storage;
  usb_ = &usb;
  sensor_ = &sensor;
  speed_ = &speed;
  speedTrend_ = &speedTrend;
  ride_ = &ride;
  battery_ = &battery;
  logger_ = &logger;
  repository_ = &repository;
  phone_ = &phone;
  settings_ = &settings;
  rainLock_.reset();
  if (storage_->loggingEnabled()) {
    String historyError;
    historyCount_ = repository_->list(*storage_, history_, 12, historyError);
  }

  String recoveryError;
  hasPendingRecovery_ = storage_->loadRecovery(pendingRecovery_, recoveryError);
  if (hasPendingRecovery_) {
    ride_->restorePaused(pendingRecovery_, millis(), sensor_->snapshot().pulseCount);
    if (storage_->loggingEnabled()) { String logError; logger_->resume(*storage_, pendingRecovery_, logError); }
    enter(Screen::Recovery);
  } else if (!storage_->sdAvailable()) {
    enter(Screen::SdMissing);
  } else {
    enter(Screen::Home);
  }
}

void UiApp::loop() {
  const uint32_t now = millis();
  if (settingsNoticeUntilMs_ != 0 &&
      static_cast<int32_t>(now - settingsNoticeUntilMs_) >= 0) {
    settingsNoticeUntilMs_ = 0;
    dirty_ = true;
  }
  const PhoneState& phoneState = phone_->state();
  const uint8_t linkState = static_cast<uint8_t>(phoneState.link);
  if (linkState != lastPhoneLinkState_) {
    lastPhoneLinkState_ = linkState;
    dirty_ = true;
  }
  if (phoneState.pairingCode != lastPairingCode_) {
    lastPairingCode_ = phoneState.pairingCode;
    if (router_.current() == Screen::Phone) dirty_ = true;
  }
  if (phone_->phoneListRevision() != lastPhoneListRevision_) {
    lastPhoneListRevision_ = phone_->phoneListRevision();
    if (router_.current() == Screen::Phone) dirty_ = true;
  }
  if (phone_->mediaRevision() != lastMediaRevision_) {
    lastMediaRevision_ = phone_->mediaRevision();
    const bool available = phone_->mediaState().available;
    if (available != lastMediaAvailable_) {
      lastMediaAvailable_ = available;
      dirty_ = true;
    }
  }
  if (phone_->navigationRevision() != lastNavigationRevision_) {
    lastNavigationRevision_ = phone_->navigationRevision();
    const bool available = phone_->navigationState().available;
    if (available != lastNavigationAvailable_) {
      lastNavigationAvailable_ = available;
      dirty_ = true;
    }
  }
  if (dirty_) {
    const uint8_t pageCount = ridePageCount();
    if (ridePage_ >= pageCount) ridePage_ = pageCount - 1;
  }
  touch_->update();
  updateModel(now);
  ui::Components::setBatteryRuntimeEstimate(battery_->remainingMinutes(),
                                            battery_->charging());
  const bool sdAvailable = storage_->sdAvailable();
  const uint8_t batteryPercent = battery_->percent();
  const int16_t batteryRemainingMinutes = battery_->remainingMinutes();
  const bool batteryCharging = battery_->charging();
  int64_t clockMinute = -1;
  if (phone_->clock().synced()) {
    clockMinute =
        (phone_->clock().epochNowMs(ClockManager::monotonicMs()) / 1000LL +
         phone_->clock().utcOffsetSeconds()) /
        60LL;
  }
  if (!headerStateInitialized_ ||
      sdAvailable != lastHeaderSdAvailable_ ||
      batteryPercent != lastHeaderBatteryPercent_ ||
      batteryRemainingMinutes != lastHeaderBatteryRemainingMinutes_ ||
      batteryCharging != lastHeaderBatteryCharging_ ||
      clockMinute != lastClockMinute_) {
    headerStateInitialized_ = true;
    lastHeaderSdAvailable_ = sdAvailable;
    lastHeaderBatteryPercent_ = batteryPercent;
    lastHeaderBatteryRemainingMinutes_ = batteryRemainingMinutes;
    lastHeaderBatteryCharging_ = batteryCharging;
    lastClockMinute_ = clockMinute;
    dirty_ = true;
  }
  rainLock_.update(touch_->point(), now);
  if (rainLock_.takeDirty()) dirty_ = true;
  bool lockEnabled = false;
  if (rainLock_.takeLockChanged(lockEnabled)) {
    Serial.print("[RAIN] ");
    Serial.println(lockEnabled ? "enabled" : "disabled");
    if (logger_->active()) {
      logger_->event(*storage_, *ride_, "RAIN_LOCK_CHANGED",
                     lockEnabled ? "enabled" : "disabled");
    }
  }

  const bool touchedNow = touch_->touched();
  if (rainLock_.blocksUi()) {
    rideTouchActive_ = false;
    rideSwipeCandidate_ = false;
    rideSwipeHandled_ = false;
    historyTouchActive_ = false;
    lastPaintValid_ = false;
    wasTouched_ = false;
  } else {
    if (router_.current() == Screen::PaintTest && touchedNow) {
      if (!wasTouched_ &&
          (headerBackHit(touch_->x(), touch_->y()) ||
           touch_->y() >= 258)) {
        handleTap(touch_->x(), touch_->y());
      } else {
        handlePaint();
      }
    } else if (router_.current() == Screen::PaintTest && !touchedNow) {
      lastPaintValid_ = false;
    }

    if (router_.current() == Screen::Ride) {
      if (touchedNow && !wasTouched_) {
        handleRideTouchStart(touch_->x(), touch_->y(), now);
      } else if (touchedNow && wasTouched_) {
        handleRideTouchMove(touch_->x(), touch_->y());
      } else if (!touchedNow && wasTouched_) {
        handleRideTouchEnd(now);
      }
    } else if (router_.current() == Screen::History) {
      if (touchedNow && !wasTouched_) {
        handleHistoryTouchStart(touch_->x(), touch_->y(), now);
      } else if (touchedNow && wasTouched_) {
        handleHistoryTouchMove(touch_->x(), touch_->y());
      } else if (!touchedNow && wasTouched_) {
        handleHistoryTouchEnd(now);
      }
    } else if (touchedNow && !wasTouched_) {
      handleTap(touch_->x(), touch_->y());
    }
    wasTouched_ = touchedNow;
  }

  const Screen currentScreen = router_.current();
  const bool phoneCountdown =
      currentScreen == Screen::Phone && phoneState.pairingCode != 0;
  const bool dynamicScreen =
      currentScreen == Screen::Ride || phoneCountdown ||
      currentScreen == Screen::TouchRawTest ||
      currentScreen == Screen::SensorTest ||
      currentScreen == Screen::BatteryTest ||
      currentScreen == Screen::SystemInfo;
  uint32_t drawInterval = settings_->uiUpdateIntervalMs;
  if (rainLock_.animationActive()) {
    drawInterval = 40;
  } else if (phoneCountdown || currentScreen == Screen::SystemInfo) {
    drawInterval = 1000;
  } else if (currentScreen == Screen::TouchRawTest) {
    drawInterval = 100;
  } else if (currentScreen == Screen::BatteryTest) {
    drawInterval = max<uint32_t>(drawInterval, 400);
  }
  if (dirty_ || (dynamicScreen && now - lastUiDrawMs_ >= drawInterval)) {
    partialRainFrame_ = rainLock_.animationActive() && !dirty_ &&
                        router_.current() == Screen::Ride;
    partialRideFrame_ = !partialRainFrame_ && !dirty_ &&
                        router_.current() == Screen::Ride;
    partialDynamicFrame_ = !partialRainFrame_ && !partialRideFrame_ &&
                           !dirty_ && dynamicScreen;
    draw();
    partialRainFrame_ = false;
    partialRideFrame_ = false;
    partialDynamicFrame_ = false;
    dirty_ = false;
    lastUiDrawMs_ = now;
  }
}

void UiApp::enter(Screen screen) {
  if (screen == Screen::SettingsWheel ||
      screen == Screen::SettingsStopThreshold ||
      screen == Screen::SettingsAutoPauseDelay ||
      screen == Screen::SettingsLogInterval ||
      screen == Screen::SettingsRgbStableRange ||
      screen == Screen::SettingsRgbStableRange5s ||
      screen == Screen::SettingsRgbStableRange10s ||
      screen == Screen::SettingsRgbBrightness) {
    settingsEdit_ = *settings_;
  }
  router_.go(screen);
  dirty_ = true;
  lastPaintValid_ = false;
  rideTouchActive_ = false;
  rideSwipeCandidate_ = false;
  rideSwipeHandled_ = false;
  historyTouchActive_ = false;
  if (screen == Screen::SdTest) {
    sdTestRun_ = false;
    sdTestResult_ = SdTestResult();
  } else if (screen == Screen::DisplayTest) {
    exactPreviewActive_ = false;
    exactPreviewIndex_ = 0;
  }
}

void UiApp::updateModel(uint32_t nowMs) {
  sensorSnapshot_ = sensor_->snapshot();
  speed_->update(sensorSnapshot_, *settings_, nowMs);
  speedTrend_->update(speed_->currentKmh(), *settings_, nowMs);
  ride_->update(nowMs, speed_->filteredKmh(), sensorSnapshot_.pulseCount, sensorSnapshot_.rejectedPulseCount);
  battery_->update(nowMs);
  recordGraphSample(nowMs);
  if (logger_->active()) {
    logger_->logSample(*storage_, *ride_, *speed_, sensorSnapshot_, *battery_, nowMs);
    logger_->retryPending(*storage_, *ride_);
    ride_->setRecoveryIdentity(logger_->rideId(), logger_->folder(), logger_->sampleIndex(), logger_->loggingGap());
    ride_->setRecoveryBattery(logger_->batteryStartVoltage(), logger_->batteryMinVoltage(), logger_->batteryMaxVoltage());
    if (battery_->state() == BatteryState::Critical && !batteryCriticalEventLogged_) { logger_->event(*storage_, *ride_, "BATTERY_CRITICAL", "battery critical"); batteryCriticalEventLogged_=true; }
    if (battery_->state() == BatteryState::Low && !batteryLowEventLogged_) { logger_->event(*storage_, *ride_, "BATTERY_LOW", "battery low"); batteryLowEventLogged_=true; }
  }
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

  const uint8_t pageCount = ridePageCount();
  if (dx < 0) {
    ridePage_ = (ridePage_ + 1) % pageCount;
  } else {
    ridePage_ = ridePage_ == 0 ? pageCount - 1 : ridePage_ - 1;
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
    const uint8_t pageCount = ridePageCount();
    if (dx < 0) {
      ridePage_ = (ridePage_ + 1) % pageCount;
    } else {
      ridePage_ = ridePage_ == 0 ? pageCount - 1 : ridePage_ - 1;
    }
    dirty_ = true;
  } else {
    handleTap(rideTouchLastX_, rideTouchLastY_);
  }

  rideTouchActive_ = false;
  rideSwipeCandidate_ = false;
  rideSwipeHandled_ = false;
}

void UiApp::handleHistoryTouchStart(int16_t x, int16_t y,
                                    uint32_t nowMs) {
  historyTouchActive_ = true;
  historyTouchStartX_ = x;
  historyTouchStartY_ = y;
  historyTouchLastX_ = x;
  historyTouchLastY_ = y;
  historyTouchStartMs_ = nowMs;
}

void UiApp::handleHistoryTouchMove(int16_t x, int16_t y) {
  if (!historyTouchActive_) return;
  historyTouchLastX_ = x;
  historyTouchLastY_ = y;
}

void UiApp::handleHistoryTouchEnd(uint32_t nowMs) {
  if (!historyTouchActive_) return;

  const int16_t dx = historyTouchLastX_ - historyTouchStartX_;
  const int16_t dy = historyTouchLastY_ - historyTouchStartY_;
  const uint32_t elapsedMs = nowMs - historyTouchStartMs_;
  const bool verticalSwipe =
      abs(dy) >= kHistorySwipeMinDy && abs(dx) <= kHistorySwipeMaxDx &&
      elapsedMs <= kHistorySwipeMaxMs && abs(dy) > abs(dx) * 2;

  if (verticalSwipe && historyCount_ > 3) {
    const uint8_t maximumOffset = historyCount_ - 3;
    if (dy < 0 && historyScrollOffset_ < maximumOffset) {
      ++historyScrollOffset_;
      historyMessage_ = String();
      dirty_ = true;
    } else if (dy > 0 && historyScrollOffset_ > 0) {
      --historyScrollOffset_;
      historyMessage_ = String();
      dirty_ = true;
    }
  } else if (!verticalSwipe) {
    handleTap(historyTouchLastX_, historyTouchLastY_);
  }

  historyTouchActive_ = false;
}

void UiApp::handleTap(int16_t x, int16_t y) {
  switch (router_.current()) {
    case Screen::SdMissing:
      if (hit(x, y, 74, 212, 156, 42)) {
        if (storage_->retry()) {
          String error;
          storage_->loadSettings(*settings_, error);
          enter(Screen::Home);
        } else {
          lastMessage_ = "SD retry failed";
          dirty_ = true;
        }
      } else if (hit(x, y, 246, 212, 160, 42)) {
        storage_->continueWithoutSaving();
        enter(Screen::Home);
      }
      break;

    case Screen::Recovery:
      if (hit(x, y, 24, 226, 144, 46)) {
        ride_->resume(millis());
        saveRecoveryIfNeeded(millis(), true);
        enter(Screen::Ride);
      } else if (hit(x, y, 180, 226, 132, 46)) {
        ride_->finish(millis());
        clearRecovery();
        enter(Screen::Ride);
      } else if (hit(x, y, 324, 226, 132, 46)) {
        clearRecovery();
        ride_->newRide(millis(), sensorSnapshot_.pulseCount);
        enter(Screen::Home);
      }
      break;

    case Screen::Home:
      if (hit(x, y, 24, 117, 432, 68)) {
        ride_->start(millis(), sensorSnapshot_.pulseCount);
        batteryLowEventLogged_ = false;
        batteryCriticalEventLogged_ = false;
        String error;
        if (storage_->loggingEnabled() &&
            logger_->start(*storage_, *settings_, *battery_, error)) {
          logger_->event(*storage_, *ride_, "START", "user started ride");
        } else if (error.length()) {
          lastMessage_ = error;
        }
        saveRecoveryIfNeeded(millis(), true);
        enter(Screen::Ride);
      } else if (hit(x, y, 24, 202, 206, 66) && storage_->sdAvailable()) {
        String error;
        historyCount_ = repository_->list(*storage_, history_, 12, error);
        historyScrollOffset_ = 0;
        historyMessage_ = error;
        enter(Screen::History);
      } else if (hit(x, y, 250, 202, 206, 66)) {
        enter(Screen::Phone);
      } else if (hit(x, y, 180, 270, 150, 50)) {
        settingsOpenedFromRide_ = false;
        enter(Screen::Settings);
      }
      break;
    case Screen::Phone:
      if (headerBackHit(x, y)) {
        enter(router_.previous() == Screen::Settings ? Screen::Settings : Screen::Home);
      } else if (phone_->state().pairingCode &&
                 hit(x, y, 150, 266, 180, 40)) {
        phone_->cancelPairing();
        dirty_ = true;
      } else if (!phone_->state().pairingCode &&
                 phone_->rememberedPhoneCount() > 0 &&
                 hit(x, y, 18, 264, 214, 42) &&
                 phone_->canRememberAnotherPhone()) {
        phone_->startPairing(millis());
        dirty_ = true;
      } else if (!phone_->state().pairingCode &&
                 phone_->rememberedPhoneCount() > 0 &&
                 hit(x, y, 248, 264, 214, 42)) {
        phone_->forgetAllPhones();
        dirty_ = true;
      } else if (phone_->rememberedPhoneCount() == 0 &&
                 hit(x, y, 106, 225, 268, 45)) {
        phone_->startPairing(millis());
        dirty_ = true;
      }
      break;
    case Screen::History:
      if (headerBackHit(x, y)) enter(Screen::Home);
      else if (historyCount_ && y >= 57) {
        const uint8_t visible =
            min<uint8_t>(historyCount_ - historyScrollOffset_, 3);
        if (y >= 57 + visible * 75) break;
        const uint8_t row = (y - 57) / 75;
        if ((y - 57) % 75 < 66) {
          historySelected_ = historyScrollOffset_ + row;
          enter(Screen::HistoryDetail);
        }
      }
      break;
    case Screen::HistoryDetail:
      if (headerBackHit(x, y) || hit(x, y, 304, 242, 176, 78)) {
        enter(Screen::History);
      } else if (hit(x, y, 8, 242, 150, 78)) {
        enter(Screen::DeleteRideConfirm);
      } else if (hit(x, y, 158, 242, 146, 78) &&
                 storage_->sdAvailable()) {
        startUsbMode();
        enter(Screen::UsbStorage);
      }
      break;
    case Screen::DeleteRideConfirm:
      if (hit(x, y, 68, 188, 177, 54)) {
        enter(Screen::HistoryDetail);
      } else if (hit(x, y, 245, 188, 167, 54) &&
                 historySelected_ < historyCount_) {
        String error;
        RideRecoveryData active = ride_->recoveryData();
        const bool removed =
            repository_->remove(*storage_, history_[historySelected_], active,
                                error);
        historyMessage_ =
            removed ? "Ride deleted" : "Delete failed: " + error;
        String listError;
        historyCount_ =
            repository_->list(*storage_, history_, 12, listError);
        if (historyCount_ <= 3) {
          historyScrollOffset_ = 0;
        } else if (historyScrollOffset_ > historyCount_ - 3) {
          historyScrollOffset_ = historyCount_ - 3;
        }
        if (listError.length()) {
          historyMessage_ = "History refresh failed: " + listError;
        }
        enter(Screen::History);
      }
      break;

    case Screen::Diagnostics:
      if (headerBackHit(x, y)) {
        enter(Screen::Settings);
      } else if (hit(x, y, 18, 57, 214, 56)) {
        enter(Screen::DisplayTest);
      } else if (hit(x, y, 248, 57, 214, 56)) {
        enter(Screen::TouchRawTest);
      } else if (hit(x, y, 18, 125, 214, 56) && storage_->sdAvailable()) {
        enter(Screen::SdTest);
      } else if (hit(x, y, 248, 125, 214, 56)) {
        enter(Screen::SensorTest);
      } else if (hit(x, y, 18, 193, 214, 56)) {
        enter(Screen::BatteryTest);
      } else if (hit(x, y, 248, 193, 214, 56)) {
        enter(Screen::SystemInfo);
      } else if (hit(x, y, 18, 266, 444, 41) && storage_->sdAvailable()) {
        startUsbMode();
        enter(Screen::UsbStorage);
      }
      break;

    case Screen::DisplayTest:
      if (!exactPreviewActive_) {
        if (headerBackHit(x, y)) {
          enter(Screen::Diagnostics);
        } else if (hit(x, y, 170, 269, 140, 38)) {
          exactPreviewActive_ = true;
          exactPreviewIndex_ = 0;
          dirty_ = true;
        }
      } else if (x < 160) {
        exactPreviewIndex_ =
            exactPreviewIndex_ == 0
                ? static_cast<uint16_t>(ui_exact::ScreenId::COUNT) - 1
                : exactPreviewIndex_ - 1;
        dirty_ = true;
      } else if (x >= 320) {
        exactPreviewIndex_ =
            (exactPreviewIndex_ + 1) %
            static_cast<uint16_t>(ui_exact::ScreenId::COUNT);
        dirty_ = true;
      } else {
        exactPreviewActive_ = false;
        dirty_ = true;
      }
      break;
    case Screen::SystemInfo:
    case Screen::SensorTest:
      if (hit(x, y, kBackX, kBackY, kBackW, kBackH) ||
          headerBackHit(x, y)) {
        enter(Screen::Diagnostics);
      }
      break;
    case Screen::TouchRawTest:
      if (hit(x, y, 14, 270, 110, 40)) {
        enter(Screen::PaintTest);
      } else if (headerBackHit(x, y) ||
                 hit(x, y, kBackX, kBackY, kBackW, kBackH)) {
        enter(Screen::Diagnostics);
      }
      break;

    case Screen::BatteryTest:
      if (headerBackHit(x, y)) enter(Screen::Diagnostics);
      else if (hit(x,y,12,262,108,58)) { settings_->batteryCalibrationFactor -= 0.005f; app::validateSettings(*settings_); battery_->updateSettings(*settings_); dirty_=true; }
      else if (hit(x,y,118,262,108,58)) { settings_->batteryCalibrationFactor += 0.005f; app::validateSettings(*settings_); battery_->updateSettings(*settings_); dirty_=true; }
      else if (hit(x,y,224,262,124,58)) { String e; const bool saved=storage_->saveSettings(*settings_,e); if(saved && logger_->active()) logger_->event(*storage_,*ride_,"CONFIG_CHANGED","battery calibration saved"); lastMessage_=saved?(e.length()?e:"Calibration saved"):e; dirty_=true; }
      else if (hit(x,y,346,262,128,58)) enter(Screen::Diagnostics);
      break;

    case Screen::SettingsRgbLed:
      if (headerBackHit(x, y)) {
        enter(Screen::Settings);
      } else if (hit(x, y, 18, 59, 444, 48)) {
        app::AppSettings candidate = *settings_;
        candidate.rgbSpeedTrendEnabled =
            !candidate.rgbSpeedTrendEnabled;
        commitSettings(candidate, "Speed LED setting saved",
                       "speed LED indicator changed", false);
        dirty_ = true;
      } else if (hit(x, y, 18, 109, 444, 48)) {
        enter(Screen::SettingsRgbStableRange);
      } else if (hit(x, y, 18, 159, 444, 48)) {
        enter(Screen::SettingsRgbStableRange5s);
      } else if (hit(x, y, 18, 209, 444, 48)) {
        enter(Screen::SettingsRgbStableRange10s);
      } else if (hit(x, y, 18, 259, 444, 48)) {
        enter(Screen::SettingsRgbBrightness);
      }
      break;

    case Screen::PaintTest:
      if (hit(x, y, 18, 270, 208, 37)) {
        dirty_ = true;
      } else if (headerBackHit(x, y) || hit(x, y, 238, 270, 224, 37)) {
        enter(Screen::Diagnostics);
      }
      break;

    case Screen::SdTest:
      if (hit(x, y, 128, 270, 224, 37)) {
        sdTestResult_ = storage_->runSdTest();
        sdTestRun_ = true;
        dirty_ = true;
      } else if (headerBackHit(x, y)) {
        enter(Screen::Diagnostics);
      }
      break;

    case Screen::UsbStorage:
      if (hit(x, y, 142, 273, 196, 34) ||
          (!usb_->active() && headerBackHit(x, y))) {
        if (usb_->active()) ESP.restart(); else enter(Screen::Home);
      }
      break;

    case Screen::Settings:
      if (headerBackHit(x, y)) {
        const bool returnToRide = settingsOpenedFromRide_;
        settingsOpenedFromRide_ = false;
        enter(returnToRide ? Screen::Ride : Screen::Home);
      }
      else if (hit(x, y, 18, 57, 214, 62)) {
        if (settingsLockedDuringRide()) showSettingsLockedNotice();
        else enter(Screen::SettingsRide);
      }
      else if (hit(x, y, 248, 57, 214, 62)) enter(Screen::SettingsDisplay);
      else if (hit(x, y, 18, 131, 214, 62)) enter(Screen::Phone);
      else if (hit(x, y, 248, 131, 214, 62)) enter(Screen::SettingsSystem);
      else if (hit(x, y, 18, 205, 214, 62)) {
        if (settingsLockedDuringRide()) showSettingsLockedNotice();
        else enter(Screen::Diagnostics);
      }
      else if (hit(x, y, 248, 205, 214, 62)) {
        if (settingsLockedDuringRide()) showSettingsLockedNotice();
        else enter(Screen::SettingsRgbLed);
      }
      break;
    case Screen::SettingsRide:
      if (headerBackHit(x, y)) enter(Screen::Settings);
      else if (hit(x, y, 18, 57, 444, 48)) enter(Screen::SettingsWheel);
      else if (hit(x, y, 18, 113, 444, 48)) enter(Screen::SettingsStopThreshold);
      else if (hit(x, y, 398, 169, 64, 48)) {
        app::AppSettings candidate = *settings_;
        candidate.autoPauseEnabled = !candidate.autoPauseEnabled;
        commitSettings(candidate, "Auto pause saved",
                       "auto pause changed", false);
        dirty_ = true;
      } else if (hit(x, y, 18, 169, 380, 48)) {
        enter(Screen::SettingsAutoPauseDelay);
      } else if (hit(x, y, 18, 225, 444, 48)) {
        enter(Screen::SettingsLogInterval);
      }
      break;
    case Screen::SettingsDisplay:
      if (headerBackHit(x, y)) enter(Screen::Settings);
      break;
    case Screen::SettingsSystem:
      if (headerBackHit(x, y)) enter(Screen::Settings);
      else if (hit(x, y, 18, 113, 444, 48) && storage_->sdAvailable()) {
        if (settingsLockedDuringRide()) {
          showSettingsLockedNotice();
        } else {
          startUsbMode();
          enter(Screen::UsbStorage);
        }
      }
      break;
    case Screen::SettingsWheel:
      if (headerBackHit(x, y)) enter(Screen::SettingsRide);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.wheelCircumferenceM =
            max(0.5f, settingsEdit_.wheelCircumferenceM - 0.005f);
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.wheelCircumferenceM =
            min(3.5f, settingsEdit_.wheelCircumferenceM + 0.005f);
      }
      else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.wheelCircumferenceM =
            settingsEdit_.wheelCircumferenceM;
        if (commitSettings(candidate, "Settings saved",
                           "wheel circumference saved", true)) {
          enter(Screen::SettingsRide);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsStopThreshold:
      if (headerBackHit(x, y)) enter(Screen::SettingsRide);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.stopThresholdKmh =
            max(0.5f, settingsEdit_.stopThresholdKmh - 0.5f);
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.stopThresholdKmh =
            min(15.0f, settingsEdit_.stopThresholdKmh + 0.5f);
      }
      else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.stopThresholdKmh = settingsEdit_.stopThresholdKmh;
        if (commitSettings(candidate, "Settings saved",
                           "stop threshold saved", true)) {
          enter(Screen::SettingsRide);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsLogInterval:
      if (headerBackHit(x, y)) enter(Screen::SettingsRide);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.logSampleIntervalMs =
            settingsEdit_.logSampleIntervalMs <= 250
                ? 250
                : settingsEdit_.logSampleIntervalMs - 250;
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.logSampleIntervalMs =
            min<uint32_t>(10000,
                          settingsEdit_.logSampleIntervalMs + 250);
      } else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.logSampleIntervalMs =
            settingsEdit_.logSampleIntervalMs;
        if (commitSettings(candidate, "Log interval saved",
                           "log interval saved", false)) {
          enter(Screen::SettingsRide);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsAutoPauseDelay:
      if (headerBackHit(x, y)) enter(Screen::SettingsRide);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.autoPauseDelayMs =
            settingsEdit_.autoPauseDelayMs <= 1000
                ? 1000
                : settingsEdit_.autoPauseDelayMs - 500;
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.autoPauseDelayMs =
            min<uint32_t>(60000,
                          settingsEdit_.autoPauseDelayMs + 500);
      } else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.autoPauseDelayMs = settingsEdit_.autoPauseDelayMs;
        if (commitSettings(candidate, "Auto pause delay saved",
                           "auto pause delay saved", false)) {
          enter(Screen::SettingsRide);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsRgbStableRange:
      if (headerBackHit(x, y)) enter(Screen::SettingsRgbLed);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.rgbSpeedTrendToleranceKmh =
            max(0.1f, settingsEdit_.rgbSpeedTrendToleranceKmh - 0.1f);
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.rgbSpeedTrendToleranceKmh =
            min(5.0f, settingsEdit_.rgbSpeedTrendToleranceKmh + 0.1f);
      } else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.rgbSpeedTrendToleranceKmh =
            settingsEdit_.rgbSpeedTrendToleranceKmh;
        if (commitSettings(candidate, "Stable range saved",
                           "speed LED stable range saved", false)) {
          enter(Screen::SettingsRgbLed);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsRgbStableRange5s:
      if (headerBackHit(x, y)) enter(Screen::SettingsRgbLed);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.rgbSpeedTrendTolerance5sKmh =
            max(0.1f, settingsEdit_.rgbSpeedTrendTolerance5sKmh - 0.1f);
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.rgbSpeedTrendTolerance5sKmh =
            min(5.0f, settingsEdit_.rgbSpeedTrendTolerance5sKmh + 0.1f);
      } else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.rgbSpeedTrendTolerance5sKmh =
            settingsEdit_.rgbSpeedTrendTolerance5sKmh;
        if (commitSettings(candidate, "5 s range saved",
                           "speed trend 5 s range saved", false)) {
          enter(Screen::SettingsRgbLed);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsRgbStableRange10s:
      if (headerBackHit(x, y)) enter(Screen::SettingsRgbLed);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.rgbSpeedTrendTolerance10sKmh =
            max(0.1f, settingsEdit_.rgbSpeedTrendTolerance10sKmh - 0.1f);
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.rgbSpeedTrendTolerance10sKmh =
            min(5.0f, settingsEdit_.rgbSpeedTrendTolerance10sKmh + 0.1f);
      } else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.rgbSpeedTrendTolerance10sKmh =
            settingsEdit_.rgbSpeedTrendTolerance10sKmh;
        if (commitSettings(candidate, "10 s range saved",
                           "speed trend 10 s range saved", false)) {
          enter(Screen::SettingsRgbLed);
        }
      }
      dirty_ = true;
      break;
    case Screen::SettingsRgbBrightness:
      if (headerBackHit(x, y)) enter(Screen::SettingsRgbLed);
      else if (hit(x, y, 74, 171, 86, 52)) {
        settingsEdit_.rgbLedBrightnessPercent =
            settingsEdit_.rgbLedBrightnessPercent <= 5
                ? 5
                : settingsEdit_.rgbLedBrightnessPercent - 5;
      } else if (hit(x, y, 320, 171, 86, 52)) {
        settingsEdit_.rgbLedBrightnessPercent =
            min<uint8_t>(100,
                         settingsEdit_.rgbLedBrightnessPercent + 5);
      } else if (hit(x, y, 150, 287, 180, 27)) {
        app::AppSettings candidate = *settings_;
        candidate.rgbLedBrightnessPercent =
            settingsEdit_.rgbLedBrightnessPercent;
        if (commitSettings(candidate, "LED brightness saved",
                           "speed LED brightness saved", false)) {
          enter(Screen::SettingsRgbLed);
        }
      }
      dirty_ = true;
      break;

    case Screen::Ride:
      if (hit(x, y, 60, 2, 44, 36)) {
        settingsOpenedFromRide_ = true;
        enter(Screen::Settings);
      } else if (hit(x, y, 321, 2, 44, 36)) {
        if (rainLock_.enable(millis())) dirty_ = true;
      } else if (rideMediaPage() && hit(x, y, 82, 201, 92, 47) &&
                 (phone_->mediaState().supportedActions &
                  media::ActionMask::Previous)) {
        phone_->sendMediaAction(media::Action::Previous);
      } else if (rideMediaPage() && hit(x, y, 190, 194, 100, 61) &&
                 (phone_->mediaState().supportedActions &
                  (media::ActionMask::Toggle | media::ActionMask::Play |
                   media::ActionMask::Pause))) {
        phone_->sendMediaAction(media::Action::Toggle);
      } else if (rideMediaPage() && hit(x, y, 306, 201, 92, 47) &&
                 (phone_->mediaState().supportedActions &
                  media::ActionMask::Next)) {
        phone_->sendMediaAction(media::Action::Next);
      } else if (hit(x, y, 24, 264, 432, 43) &&
          (ride_->state() == RideState::IDLE || ride_->state() == RideState::FINISHED)) {
        ride_->start(millis(), sensorSnapshot_.pulseCount);
        batteryLowEventLogged_ = false; batteryCriticalEventLogged_ = false;
        String error; if (storage_->loggingEnabled() && logger_->start(*storage_, *settings_, *battery_, error)) logger_->event(*storage_, *ride_, "START", "user started ride"); else if (error.length()) lastMessage_=error;
        saveRecoveryIfNeeded(millis(), true);
        dirty_ = true;
      } else if (hit(x, y, 24, 266, 291, 41)) {
        if (ride_->state() == RideState::RIDING) {
          logger_->event(*storage_, *ride_, "PAUSE", "user paused ride");
          ride_->pause(millis());
        } else if (ride_->state() == RideState::PAUSED) {
          ride_->resume(millis());
          logger_->event(*storage_, *ride_, "RESUME", "user resumed ride");
        }
        saveRecoveryIfNeeded(millis(), true);
        dirty_ = true;
      } else if (hit(x, y, 327, 266, 129, 41)) {
        if (ride_->state() == RideState::RIDING || ride_->state() == RideState::PAUSED) {
          enter(Screen::FinishConfirm);
        }
        dirty_ = true;
      }
      break;
    case Screen::FinishConfirm:
      if (hit(x,y,78,198,157,34)) enter(Screen::Ride);
      else if (hit(x,y,245,198,157,34)) { const uint32_t now=millis(); ride_->update(now,speed_->filteredKmh(),sensorSnapshot_.pulseCount,sensorSnapshot_.rejectedPulseCount); ride_->finish(now); String error; if(!logger_->finish(*storage_,*ride_,*battery_,error)) lastMessage_=error; else clearRecovery(); enter(Screen::RideSummary); }
      break;
    case Screen::RideSummary:
      if(hit(x,y,24,265,194,42)) enter(Screen::Home);
      else if(hit(x,y,230,265,226,42)) {
        String error;
        historyCount_ = repository_->list(*storage_, history_, 12, error);
        historyScrollOffset_ = 0;
        historyMessage_ = error;
        enter(Screen::History);
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
  if (partialRainFrame_ || partialRideFrame_ || partialDynamicFrame_) {
    display_->beginPartialFrame();
  } else {
    display_->beginFrame();
  }
  switch (router_.current()) {
    case Screen::SdMissing:
      drawSdMissing();
      break;
    case Screen::Recovery:
      drawRecovery();
      break;
    case Screen::Home:
      drawMainMenu();
      break;
    case Screen::Phone:
      drawPhone();
      break;
    case Screen::History:
      drawHistory();
      break;
    case Screen::HistoryDetail:
      drawHistoryDetail();
      break;
    case Screen::DeleteRideConfirm:
      drawDeleteRideConfirm();
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
    case Screen::SettingsRide:
      drawSettingsRide();
      break;
    case Screen::SettingsDisplay:
      drawSettingsDisplay();
      break;
    case Screen::SettingsSystem:
      drawSettingsSystem();
      break;
    case Screen::SettingsWheel:
      drawSettingsWheel();
      break;
    case Screen::SettingsStopThreshold:
      drawSettingsStopThreshold();
      break;
    case Screen::SettingsAutoPauseDelay:
      drawSettingsAutoPauseDelay();
      break;
    case Screen::SettingsLogInterval:
      drawSettingsLogInterval();
      break;
    case Screen::SettingsRgbLed:
      drawSettingsRgbLed();
      break;
    case Screen::SettingsRgbStableRange:
      drawSettingsRgbStableRange();
      break;
    case Screen::SettingsRgbStableRange5s:
      drawSettingsRgbStableRange5s();
      break;
    case Screen::SettingsRgbStableRange10s:
      drawSettingsRgbStableRange10s();
      break;
    case Screen::SettingsRgbBrightness:
      drawSettingsRgbBrightness();
      break;
    case Screen::Ride:
      drawRide();
      break;
    case Screen::FinishConfirm:
      drawFinishConfirm();
      break;
    case Screen::RideSummary:
      drawRideSummary();
      break;
  }
  if (partialRainFrame_) {
    display_->commitFrameArea(60, 60, 360, 186);
  } else if (partialRideFrame_) {
    // Static header/footer stay on the panel; only the live content band is
    // transferred at telemetry cadence.
    display_->commitFrameArea(18, 44, 444, 214);
  } else if (partialDynamicFrame_) {
    switch (router_.current()) {
      case Screen::Phone:
        display_->commitFrameArea(96, 238, 288, 26);
        break;
      case Screen::TouchRawTest:
        display_->commitFrameArea(18, 58, 444, 196);
        break;
      case Screen::SensorTest:
        display_->commitFrameArea(156, 58, 306, 206);
        break;
      case Screen::BatteryTest:
        display_->commitFrameArea(18, 58, 444, 196);
        break;
      case Screen::SystemInfo:
        display_->commitFrameArea(270, 48, 190, 226);
        break;
      default:
        display_->commitFrame();
        break;
    }
  } else {
    display_->commitFrame();
  }
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

String UiApp::storageStatusShort() const {
  if (storage_->usbModeActive()) {
    return "USB";
  }
  return storage_->sdAvailable() ? "SD OK" : "NO SD";
}

void UiApp::drawSdMissing() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_01_SD_MISSING);
  ui::HeaderStatus header;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = false;
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Storage", header);
  if (lastMessage_.length()) {
    tft.fillRect(80, 174, 320, 22, ui::BG);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ui::DANGER, ui::BG);
    tft.drawString(lastMessage_, 240, 185, 1);
  }
}

void UiApp::drawRecovery() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_02_RECOVERY);
  ui::HeaderStatus header;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Recovery", header);
  const RideStats& stats = ride_->stats();
  ui::Components::card(tft, 24, 119, 122, 75, "DISTANCE",
                       String(stats.distanceM / 1000.0f, 1), "km");
  ui::Components::card(tft, 158, 119, 122, 75, "MOVING",
                       durationText(stats.movingMs).substring(3));
  ui::Components::card(tft, 292, 119, 164, 75, "AVG SPEED",
                       String(stats.averageMovingSpeedKmh, 1), "km/h");
}

void UiApp::drawMainMenu() {
  ui::HomeViewModel model;
  model.header.phoneConnected = phone_->ready();
  model.header.sdAvailable = storage_->sdAvailable();
  model.header.batteryAvailable = battery_->enabled();
  model.header.batteryPercent = battery_->percent();
  model.phoneConnected = phone_->ready();
  model.historyAvailable = storage_->sdAvailable();
  model.rideCount = historyCount_;
  model.timeText = currentClockText();
  ui::HomeScreen::draw(display_->tft(), model);
}

void UiApp::drawPhone() {
  TFT_eSPI& tft = display_->tft();
  const PhoneState& phone = phone_->state();
  const uint8_t rememberedCount = phone_->rememberedPhoneCount();
  const ui_exact::ScreenId source =
      phone.pairingCode
          ? ui_exact::ScreenId::SCREEN_31_PHONE_PAIRING
          : (!rememberedCount
                 ? ui_exact::ScreenId::SCREEN_30_PHONE_UNPAIRED
                 : (phone_->ready()
                        ? ui_exact::ScreenId::SCREEN_32_PHONE_CONNECTED
                        : ui_exact::ScreenId::SCREEN_35_PHONE_LOST));
  ui_exact::ExactScreenRenderer(tft).draw(source);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, phone.pairingCode ? "Pair phone" : "Phone",
                         header);
  tft.setTextDatum(MC_DATUM);
  if (phone.pairingCode) {
    tft.fillRect(96, 195, 288, 112, ui::BG);
    tft.fillRoundRect(115, 198, 250, 42, 12, ui::SURFACE);
    tft.drawRoundRect(115, 198, 250, 42, 12, ui::BORDER);
    char code[20];
    snprintf(code, sizeof(code), "Code  %03lu %03lu",
             static_cast<unsigned long>(phone.pairingCode / 1000),
             static_cast<unsigned long>(phone.pairingCode % 1000));
    tft.setTextColor(ui::ACCENT, ui::SURFACE);
    tft.drawString(code, 240, 219, 4);
    const uint32_t remainingMs =
        static_cast<int32_t>(phone.pairingExpiresMs - millis()) > 0
            ? phone.pairingExpiresMs - millis()
            : 0;
    char expires[24];
    snprintf(expires, sizeof(expires), "Expires in %02lu:%02lu",
             static_cast<unsigned long>(remainingMs / 60000UL),
             static_cast<unsigned long>((remainingMs / 1000UL) % 60UL));
    tft.setTextColor(ui::TEXT_MUTED, ui::BG);
    tft.drawString(expires, 240, 252, 1);
    ui::Components::button(tft, 150, 266, 180, 40, "Cancel", false, true,
                           true);
  } else if (rememberedCount) {
    tft.fillRect(0, 40, 480, 280, ui::BG);
    for (uint8_t i = 0; i < rememberedCount; ++i) {
      const RememberedPhone* remembered = phone_->rememberedPhone(i);
      if (!remembered) continue;
      const int16_t y = 48 + i * 50;
      const bool connected =
          phone_->ready() &&
          remembered->associationId == phone_->associationId();
      tft.fillRoundRect(18, y, 444, 43, 10, ui::SURFACE);
      tft.drawRoundRect(18, y, 444, 43, 10,
                        connected ? ui::SUCCESS : ui::BORDER);
      tft.fillCircle(34, y + 21, 4,
                     connected ? ui::SUCCESS : ui::TEXT_MUTED);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(ui::TEXT, ui::SURFACE);
      tft.drawString(remembered->displayName[0]
                         ? remembered->displayName
                         : "Android phone",
                     48, y + 21, 2);
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(connected ? ui::SUCCESS : ui::TEXT_MUTED,
                       ui::SURFACE);
      tft.drawString(connected ? "Connected" : "Remembered", 448,
                     y + 21, 1);
    }
    ui::Components::button(
        tft, 18, 264, 214, 42,
        phone_->canRememberAnotherPhone() ? "Add phone" : "List full",
        true, false, phone_->canRememberAnotherPhone());
    ui::Components::button(tft, 248, 264, 214, 42, "Forget all", false,
                           true, true);
  }
}

void UiApp::drawHistory() {
  ui::HeaderStatus header;
  header.showBack = true;
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::SecondaryScreens::history(display_->tft(), header, history_,
                                historyCount_, historyScrollOffset_,
                                historyMessage_);
}
void UiApp::drawHistoryDetail() {
  ui::HeaderStatus header;
  header.showBack = true;
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::SecondaryScreens::historyDetail(
      display_->tft(), header,
      historySelected_ < historyCount_ ? &history_[historySelected_] : nullptr,
      storage_->sdAvailable() && !storage_->usbModeActive());
}

void UiApp::drawDeleteRideConfirm() {
  drawHistoryDetail();
  ui::SecondaryScreens::deleteRideConfirm(display_->tft());
}

void UiApp::drawDiagnostics() {
  ui::HeaderStatus header;
  header.showBack = true;
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::SecondaryScreens::diagnostics(display_->tft(), header, storage_->sdAvailable());
}

void UiApp::drawDisplayTest() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer exact(tft);
  if (exactPreviewActive_) {
    const ui_exact::ScreenId id =
        static_cast<ui_exact::ScreenId>(exactPreviewIndex_);
    exact.draw(id);
    Serial.print("[UI] exact preview ");
    Serial.println(ui_exact::getScreenAsset(id).name);
    return;
  }
  exact.draw(ui_exact::ScreenId::SCREEN_81_DISPLAY_TEST);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Display test", header);
}

void UiApp::drawTouchRawTest() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_82_TOUCH_RAW);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Touch raw", header);
  const TouchPoint& p = touch_->point();
  ui::Components::card(tft, 18, 58, 142, 72, "X", String(p.x));
  ui::Components::card(tft, 170, 58, 142, 72, "Y", String(p.y));
  ui::Components::card(tft, 322, 58, 140, 72, "POINTS",
                       String(p.points));
  tft.fillRect(18, 134, 444, 20, ui::BG);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(p.intLevel ? ui::TEXT : ui::SUCCESS, ui::BG);
  tft.drawString(String("INT: ") + (p.intLevel ? "HIGH" : "LOW"), 20, 144, 1);
  const uint32_t age = p.lastTouchMs ? millis() - p.lastTouchMs : 0;
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(ui::TEXT, ui::BG);
  tft.drawString(p.lastTouchMs ? String(age) + " ms ago" : "Never", 458,
                 144, 1);

  constexpr int16_t fieldX = 18;
  constexpr int16_t fieldY = 156;
  constexpr int16_t fieldW = 444;
  constexpr int16_t fieldH = 98;
  tft.fillRoundRect(fieldX, fieldY, fieldW, fieldH, 10, ui::SURFACE);
  tft.drawRoundRect(fieldX, fieldY, fieldW, fieldH, 10, ui::BORDER);
  tft.drawFastVLine(fieldX + fieldW / 2, fieldY + 7, fieldH - 14, ui::BORDER);
  tft.drawFastHLine(fieldX + 7, fieldY + fieldH / 2, fieldW - 14, ui::BORDER);
  for (uint8_t i = 0; i < 2; ++i) {
    const TouchContact& contact = p.contacts[i];
    if (!contact.valid) continue;
    const uint16_t color = i == 0 ? ui::UI_CYAN : ui::UI_ORANGE;
    const int16_t px = fieldX + 7 +
                       (contact.x * (fieldW - 15)) /
                           (hw::DISPLAY_WIDTH - 1);
    const int16_t py = fieldY + 7 +
                       (contact.y * (fieldH - 15)) /
                           (hw::DISPLAY_HEIGHT - 1);
    tft.drawCircle(px, py, 7, color);
    tft.fillCircle(px, py, 3, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(color, ui::SURFACE);
    tft.drawString(String(i + 1), px, py - 13, 1);
  }
  drawSoftButton(14, 270, 110, 40, "PAINT", ui::UI_CYAN);
  drawBackButton();
}

void UiApp::drawPaintTest() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_83_PAINT_TEST);
  tft.fillRect(0, 40, display_->width(), 220, ui::BG);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Paint test", header);
}

void UiApp::drawSdTest() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_84_SD_TEST);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "SD test", header);
  tft.fillRect(18, 52, 444, 207, ui::BG);
  drawTextBlock(storage_->sdInfoText(), 24, 56, 14,
                storage_->sdAvailable() ? ui::UI_TEXT : ui::UI_RED,
                ui::UI_BG, 1);
  if (sdTestRun_) {
    drawTextBlock(sdTestResult_.message, 24, 160, 18,
                  sdTestResult_.ok ? ui::UI_GREEN : ui::UI_RED,
                  ui::UI_BG, 1);
    drawTextBlock(sdTestResult_.readBack.substring(0, 180), 24, 198, 15,
                  ui::UI_MUTED, ui::UI_BG, 1);
  } else {
    drawTextBlock(
        "Press Run to test root write/read\nusing /SDTEST.TXT.", 24,
        176, 18, ui::UI_MUTED, ui::UI_BG, 1);
  }
  ui::Components::button(
      tft, 128, 270, 224, 37, sdTestRun_ ? "Run again" : "Run test", true,
      false, storage_->sdAvailable() && !storage_->usbModeActive());
}

void UiApp::drawUsbStorage() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_88_USB_STORAGE);
  ui::HeaderStatus header;
  header.showBack = !usb_->active();
  header.phoneConnected = false;
  header.sdAvailable = false;
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "USB Storage", header);
  if (usb_->active()) return;
  tft.fillRect(20, 60, 440, 205, ui::BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(ui::UI_RED, ui::UI_BG);
  tft.drawString("USB Storage Error", 240, 96, 4);
  tft.setTextColor(ui::UI_MUTED, ui::UI_BG);
  tft.drawString(usb_->status(), 240, 142, 2);
  tft.drawString("SD: " + storageStatusShort(), 240, 170, 2);
}

void UiApp::drawSensorTest() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_85_SENSOR_TEST);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Sensor test", header);
  ui::Components::card(tft, 18, 58, 128, 70, "GPIO",
                       String(hw::PIN_HALL_SENSOR));
  ui::Components::card(tft, 156, 58, 148, 70, "PULSES",
                       String(sensorSnapshot_.pulseCount), String(), true);
  ui::Components::card(tft, 314, 58, 148, 70, "LEVEL",
                       sensorSnapshot_.pinLevel == HIGH ? "HIGH" : "LOW");
  tft.fillRect(300, 140, 160, 132, ui::BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(ui::TEXT, ui::BG);
  tft.drawString(String(sensorSnapshot_.lastIntervalUs / 1000ULL) + " ms",
                 456, 159, 2);
  tft.setTextColor(ui::ACCENT, ui::BG);
  tft.drawString(String(speed_->filteredKmh(), 1) + " km/h", 456, 191, 2);
  tft.setTextColor(ui::TEXT, ui::BG);
  tft.drawString(String(sensorSnapshot_.rejectedPulseCount), 456, 223, 2);
  tft.drawString(app::interruptModeToString(settings_->sensorInterruptMode),
                 456, 255, 2);
}

void UiApp::drawBatteryTest() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_86_BATTERY_TEST);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "Battery test", header);
  ui::Components::card(tft, 18, 58, 170, 78, "BATTERY",
                       String(battery_->voltage(), 2), "V", true);
  ui::Components::card(tft, 198, 58, 132, 78, "CHARGE",
                       String(battery_->percent()), "%");
  ui::Components::card(tft, 340, 58, 122, 78, "ADC",
                       String(battery_->rawAdc()));
  tft.fillRect(300, 150, 160, 103, ui::BG);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(ui::TEXT, ui::BG);
  tft.drawString(String(battery_->adcMillivolts() / 1000.0f, 3) + " V",
                 456, 171, 2);
  tft.drawString(String(hw::BATTERY_VOLTAGE_DIVIDER_RATIO, 3), 456, 204, 2);
  tft.drawString(String(settings_->batteryCalibrationFactor, 3), 456, 237,
                 2);
  // Clear the sourcepack's single Calibrate control before laying out the
  // four real calibration actions. The old controls visually overlapped it.
  tft.fillRect(0, 262, display_->width(), 58, ui::BG);
  ui::Components::button(tft, 18, 270, 96, 37, "Cal -");
  ui::Components::button(tft, 124, 270, 96, 37, "Cal +");
  ui::Components::button(tft, 230, 270, 112, 37, "Save", true);
  ui::Components::button(tft, 352, 270, 110, 37, "Back");
}

void UiApp::drawSystemInfo() {
  TFT_eSPI& tft = display_->tft();
  ui_exact::ExactScreenRenderer(tft).draw(
      ui_exact::ScreenId::SCREEN_87_SYSTEM_INFO);
  ui::HeaderStatus header;
  header.showBack = true;
  header.phoneConnected = phone_->ready();
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  ui::Components::header(tft, "System info", header);
  const String values[] = {
      app::FIRMWARE_VERSION,
      app::BOARD_NAME,
      flashSizeText() + " / " + psramText(),
      String(app::DISPLAY_NAME) + " - 480x320",
      touch_->isReady() ? app::TOUCH_NAME : "Not found",
      phone_->ready() ? "Connected" : "Disconnected",
      String(ESP.getFreeHeap() / 1024UL) + " KB",
  };
  const int16_t ys[] = {58, 92, 126, 160, 194, 228, 262};
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(ui::TEXT, ui::BG);
  for (uint8_t i = 0; i < 7; ++i) {
    tft.fillRect(270, ys[i] - 10, 190, 22, ui::BG);
    tft.drawString(values[i], 456, ys[i], 2);
  }
}

void UiApp::drawSettings() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  status.sdText = storage_->statusText();
  status.usbActive = usb_->active();
  status.rideActive = settingsLockedDuringRide();
  status.showRideLockNotice = settingsNoticeUntilMs_ != 0;
  ui::SettingsScreen::drawMain(display_->tft(), status);
}

void UiApp::drawSettingsRide() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawRide(display_->tft(), status, *settings_);
}

void UiApp::drawSettingsDisplay() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawDisplay(display_->tft(), status);
}

void UiApp::drawSettingsSystem() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  status.sdText = storage_->statusText();
  status.usbActive = usb_->active();
  status.timeSource = phone_->clock().synced() ? "Phone" : "Unavailable";
  status.rideActive = settingsLockedDuringRide();
  status.showRideLockNotice = settingsNoticeUntilMs_ != 0;
  ui::SettingsScreen::drawSystem(display_->tft(), status);
}

void UiApp::drawSettingsWheel() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawWheel(display_->tft(), status, settingsEdit_);
}

void UiApp::drawSettingsStopThreshold() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawStopThreshold(display_->tft(), status,
                                        settingsEdit_);
}

void UiApp::drawSettingsAutoPauseDelay() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawAutoPauseDelay(display_->tft(), status,
                                         settingsEdit_);
}

void UiApp::drawSettingsLogInterval() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawLogInterval(display_->tft(), status,
                                      settingsEdit_);
}

void UiApp::drawSettingsRgbLed() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawRgbLed(display_->tft(), status, *settings_);
}

void UiApp::drawSettingsRgbStableRange() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawRgbStableRange(display_->tft(), status,
                                         settingsEdit_);
}

void UiApp::drawSettingsRgbStableRange5s() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawRgbStableRange5s(display_->tft(), status,
                                           settingsEdit_);
}

void UiApp::drawSettingsRgbStableRange10s() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawRgbStableRange10s(display_->tft(), status,
                                            settingsEdit_);
}

void UiApp::drawSettingsRgbBrightness() {
  ui::SettingsStatus status;
  status.header.showBack = true;
  status.header.sdAvailable = storage_->sdAvailable();
  status.header.batteryAvailable = battery_->enabled();
  status.header.batteryPercent = battery_->percent();
  ui::SettingsScreen::drawRgbBrightness(display_->tft(), status,
                                        settingsEdit_);
}

void UiApp::drawRide() {
  ui::RideViewModel model;
  model.header.showRain = true;
  model.header.showSettings = true;
  model.header.rainLocked = rainLock_.locked();
  model.header.phoneConnected = phone_->ready();
  model.header.sdAvailable = storage_->sdAvailable();
  model.header.batteryAvailable = battery_->enabled();
  model.header.batteryPercent = battery_->percent();
  model.state = ride_->state();
  model.stats = &ride_->stats();
  model.speedKmh = speed_->currentKmh();
  model.graphSamples = graphSamples_;
  model.graphCount = 120;
  model.graphStart = graphWriteIndex_;
  model.graphWindowSeconds = settings_->graphWindowSeconds;
  model.page = ridePage_;
  model.pageCount = ridePageCount();
  model.trendPageEnabled = settings_->rgbSpeedTrendEnabled;
  model.speedTrend = speedTrend_ ? &speedTrend_->snapshot() : nullptr;
  model.navigation = &phone_->navigationState();
  model.media = &phone_->mediaState();
  model.epochNowMs =
      phone_->clock().epochNowMs(ClockManager::monotonicMs());
  model.utcOffsetSeconds = phone_->clock().utcOffsetSeconds();
  model.timeText = currentClockText();
  model.renderMode =
      partialRainFrame_
          ? ui::RideRenderMode::RainRegion
          : (partialRideFrame_ ? ui::RideRenderMode::ContentRegion
                               : ui::RideRenderMode::Full);
  ui::RideScreen::draw(display_->tft(), model);
  ui::RainLockOverlay::draw(display_->tft(), rainLock_);
}

uint8_t UiApp::ridePageCount() const {
  return 3 + (settings_ && settings_->rgbSpeedTrendEnabled ? 1 : 0) +
         (phone_ && phone_->navigationState().available ? 1 : 0) +
         (phone_ && phone_->mediaState().available ? 1 : 0);
}

bool UiApp::rideMediaPage() const {
  if (!phone_ || !phone_->mediaState().available) return false;
  const uint8_t index =
      3 + (settings_ && settings_->rgbSpeedTrendEnabled ? 1 : 0) +
      (phone_->navigationState().available ? 1 : 0);
  return ridePage_ == index;
}

String UiApp::currentClockText() const {
  if (!phone_ || !phone_->clock().synced()) return "--:--";
  int64_t seconds =
      phone_->clock().epochNowMs(ClockManager::monotonicMs()) / 1000LL +
      phone_->clock().utcOffsetSeconds();
  seconds %= 86400LL;
  if (seconds < 0) seconds += 86400LL;
  char text[8];
  snprintf(text, sizeof(text), "%02lld:%02lld",
           static_cast<long long>(seconds / 3600LL),
           static_cast<long long>((seconds / 60LL) % 60LL));
  return String(text);
}

void UiApp::drawFinishConfirm() {
  // The exact source defines this as a dimmed ride screen. Draw the current
  // runtime page first so sourcepack sample speed/distance never leak into UI.
  drawRide();
  ui::SecondaryScreens::finishConfirm(display_->tft());
}
void UiApp::drawRideSummary() {
  ui::HeaderStatus header;
  header.sdAvailable = storage_->sdAvailable();
  header.batteryAvailable = battery_->enabled();
  header.batteryPercent = battery_->percent();
  header.phoneConnected = phone_->ready();
  ui::SecondaryScreens::rideSummary(display_->tft(), header, ride_->stats());
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

String UiApp::durationText(uint64_t ms) const {
  const uint64_t totalSeconds = ms / 1000ULL;
  const uint64_t hours = totalSeconds / 3600ULL;
  const uint64_t minutes = (totalSeconds / 60ULL) % 60ULL;
  const uint64_t seconds = totalSeconds % 60ULL;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu", static_cast<unsigned long long>(hours),
           static_cast<unsigned long long>(minutes), static_cast<unsigned long long>(seconds));
  return String(buf);
}

void UiApp::startUsbMode() {
  if (ride_->state() == RideState::RIDING) {
    lastMessage_ = "Active ride: pause or finish first";
    return;
  }
  if (ride_->state() == RideState::PAUSED) saveRecoveryIfNeeded(millis(), true);
  phone_->prepareForUsb();
  if (logger_->active()) logger_->event(*storage_, *ride_, "USB_STORAGE_REQUESTED", "USB Mass Storage requested");
  logger_->close();
  usb_->begin(*storage_);
  dirty_ = true;
}

bool UiApp::settingsLockedDuringRide() const {
  return ride_->state() == RideState::RIDING ||
         ride_->state() == RideState::PAUSED;
}

void UiApp::showSettingsLockedNotice() {
  settingsNoticeUntilMs_ = millis() + 2600;
  dirty_ = true;
}

bool UiApp::commitSettings(const app::AppSettings& candidate,
                           const char* successMessage,
                           const char* eventDetails, bool updateSensor) {
  app::AppSettings validated = candidate;
  app::validateSettings(validated);
  String error;
  if (!storage_->saveSettings(validated, error)) {
    lastMessage_ = error;
    return false;
  }
  *settings_ = validated;
  lastMessage_ = error.length() ? error : successMessage;
  const uint8_t pageCount = ridePageCount();
  if (pageCount && ridePage_ >= pageCount) ridePage_ = pageCount - 1;
  if (updateSensor) sensor_->updateSettings(*settings_);
  if (logger_->active() && eventDetails) {
    logger_->event(*storage_, *ride_, "CONFIG_CHANGED", eventDetails);
  }
  return true;
}

void UiApp::saveRecoveryIfNeeded(uint32_t nowMs, bool force) {
  if (!storage_->loggingEnabled()) {
    return;
  }
  if (!force && !ride_->needsRecoverySave(nowMs, settings_->recoveryIntervalMs)) {
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
  uint32_t period = settings_->graphWindowSeconds * 1000UL / 120UL;
  if (period < 100) period = 100;
  if (nowMs - lastGraphSampleMs_ < period) {
    return;
  }
  lastGraphSampleMs_ = nowMs;
  graphSamples_[graphWriteIndex_] = speed_->currentKmh();
  graphWriteIndex_ = (graphWriteIndex_ + 1) % 120;
}
