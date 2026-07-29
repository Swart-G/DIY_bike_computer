#pragma once

#include <Arduino.h>

#include "touch/TouchManager.h"

enum class RainLockState : uint8_t {
  Disabled,
  LockedIdle,
  Priming,
  Holding,
  UnlockSuccess,
};

class RainLockManager {
 public:
  static constexpr int16_t kLeftX = 160;
  static constexpr int16_t kRightX = 320;
  static constexpr int16_t kTargetY = 192;
  static constexpr int16_t kTargetRadius = 36;
  static constexpr uint32_t kPreHoldDurationMs = 2000;
  static constexpr uint32_t kHoldDurationMs = 3000;
  static constexpr uint32_t kEnableToastMs = 1300;
  static constexpr uint32_t kHintToastMs = 1600;
  static constexpr uint32_t kSuccessMs = 650;

  void reset();
  bool enable(uint32_t nowMs);
  void update(const TouchPoint& frame, uint32_t nowMs);

  RainLockState state() const { return state_; }
  bool locked() const;
  bool blocksUi() const { return state_ != RainLockState::Disabled; }
  bool overlayVisible() const;
  bool enableToastVisible() const;
  bool hintVisible() const;
  bool successVisible() const { return state_ == RainLockState::UnlockSuccess; }
  bool animationActive() const { return overlayVisible(); }
  uint32_t preHoldElapsedMs() const { return preHoldElapsedMs_; }
  float progress() const { return progress_; }
  uint32_t holdElapsedMs() const { return holdElapsedMs_; }
  uint32_t nowMs() const { return nowMs_; }

  bool takeDirty();
  bool takeLockChanged(bool& locked);

 private:
  bool contactInTarget(const TouchContact& contact, int16_t targetX) const;
  bool findInitialTargets(const TouchPoint& frame, const TouchContact*& left,
                          const TouchContact*& right) const;
  bool validateTrackedTargets(const TouchPoint& frame, const TouchContact*& left,
                              const TouchContact*& right) const;
  const TouchContact* findById(const TouchPoint& frame, uint8_t id) const;
  bool movementPlausible(const TouchContact& contact, int16_t lastX, int16_t lastY) const;
  void showHint(uint32_t nowMs);
  void startPriming(const TouchContact& left, const TouchContact& right,
                    uint32_t nowMs);
  void startHolding(const TouchContact& left, const TouchContact& right, uint32_t nowMs);
  void abortUnlock(uint32_t nowMs);
  void clearTracking();
  void completeUnlock(uint32_t nowMs);

  RainLockState state_ = RainLockState::Disabled;
  uint32_t nowMs_ = 0;
  uint32_t toastUntilMs_ = 0;
  uint32_t hintUntilMs_ = 0;
  uint32_t preHoldStartMs_ = 0;
  uint32_t preHoldElapsedMs_ = 0;
  uint32_t holdStartMs_ = 0;
  uint32_t holdElapsedMs_ = 0;
  uint32_t successStartedMs_ = 0;
  float progress_ = 0.0f;
  uint8_t leftId_ = 0xFF;
  uint8_t rightId_ = 0xFF;
  int16_t lastLeftX_ = 0;
  int16_t lastLeftY_ = 0;
  int16_t lastRightX_ = 0;
  int16_t lastRightY_ = 0;
  bool lastTouched_ = false;
  bool dirty_ = true;
  bool lockChangePending_ = false;
  bool changedLockValue_ = false;
};
