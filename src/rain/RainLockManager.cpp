#include "rain/RainLockManager.h"

namespace {

constexpr int16_t kMaximumTrackedStepPx = 72;

uint32_t distanceSquared(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  const int32_t dx = static_cast<int32_t>(x0) - x1;
  const int32_t dy = static_cast<int32_t>(y0) - y1;
  return static_cast<uint32_t>(dx * dx + dy * dy);
}

}  // namespace

void RainLockManager::reset() {
  state_ = RainLockState::Disabled;
  nowMs_ = 0;
  toastUntilMs_ = 0;
  hintUntilMs_ = 0;
  preHoldStartMs_ = 0;
  preHoldElapsedMs_ = 0;
  holdStartMs_ = 0;
  holdElapsedMs_ = 0;
  successStartedMs_ = 0;
  progress_ = 0.0f;
  leftId_ = rightId_ = 0xFF;
  lastTouched_ = false;
  dirty_ = true;
  lockChangePending_ = false;
  changedLockValue_ = false;
}

bool RainLockManager::enable(uint32_t nowMs) {
  if (state_ != RainLockState::Disabled) return false;
  nowMs_ = nowMs;
  state_ = RainLockState::LockedIdle;
  hintUntilMs_ = 0;
  clearTracking();
  toastUntilMs_ = nowMs + kEnableToastMs;
  // The enabling tap must not immediately trigger the compact hint or begin
  // the two-point pre-hold while the same finger is still on the panel.
  lastTouched_ = true;
  dirty_ = true;
  lockChangePending_ = true;
  changedLockValue_ = true;
  return true;
}

void RainLockManager::update(const TouchPoint& frame, uint32_t nowMs) {
  nowMs_ = nowMs;
  if (state_ == RainLockState::Disabled) {
    lastTouched_ = frame.touched;
    return;
  }

  if (state_ == RainLockState::LockedIdle) {
    if (toastUntilMs_ != 0 && static_cast<int32_t>(nowMs - toastUntilMs_) >= 0) {
      toastUntilMs_ = 0;
      dirty_ = true;
    }
    if (hintUntilMs_ != 0 &&
        static_cast<int32_t>(nowMs - hintUntilMs_) >= 0) {
      hintUntilMs_ = 0;
      dirty_ = true;
    }
    if (toastUntilMs_ == 0) {
      const TouchContact* left = nullptr;
      const TouchContact* right = nullptr;
      if (findInitialTargets(frame, left, right)) {
        startPriming(*left, *right, nowMs);
      } else if (frame.touched && !lastTouched_) {
        showHint(nowMs);
      }
    }
  } else if (state_ == RainLockState::Priming) {
    const TouchContact* left = nullptr;
    const TouchContact* right = nullptr;
    if (validateTrackedTargets(frame, left, right)) {
      lastLeftX_ = left->x;
      lastLeftY_ = left->y;
      lastRightX_ = right->x;
      lastRightY_ = right->y;
      preHoldElapsedMs_ = nowMs - preHoldStartMs_;
      if (preHoldElapsedMs_ >= kPreHoldDurationMs) {
        startHolding(*left, *right, nowMs);
      }
    } else {
      abortUnlock(nowMs);
    }
  } else if (state_ == RainLockState::Holding) {
    const TouchContact* left = nullptr;
    const TouchContact* right = nullptr;
    if (validateTrackedTargets(frame, left, right)) {
      lastLeftX_ = left->x;
      lastLeftY_ = left->y;
      lastRightX_ = right->x;
      lastRightY_ = right->y;
      holdElapsedMs_ = nowMs - holdStartMs_;
      progress_ =
          constrain(holdElapsedMs_ / static_cast<float>(kHoldDurationMs),
                    0.0f, 1.0f);
      if (holdElapsedMs_ >= kHoldDurationMs) completeUnlock(nowMs);
    } else {
      abortUnlock(nowMs);
    }
  } else if (state_ == RainLockState::UnlockSuccess) {
    if (nowMs - successStartedMs_ >= kSuccessMs && !frame.touched) {
      state_ = RainLockState::Disabled;
      dirty_ = true;
    }
  }
  lastTouched_ = frame.touched;
}

bool RainLockManager::locked() const {
  return state_ == RainLockState::LockedIdle ||
         state_ == RainLockState::Priming ||
         state_ == RainLockState::Holding;
}

bool RainLockManager::overlayVisible() const {
  return state_ == RainLockState::Holding;
}

bool RainLockManager::enableToastVisible() const {
  return state_ == RainLockState::LockedIdle && toastUntilMs_ != 0;
}

bool RainLockManager::hintVisible() const {
  return state_ == RainLockState::Priming ||
         (state_ == RainLockState::LockedIdle && hintUntilMs_ != 0);
}

bool RainLockManager::takeDirty() {
  const bool value = dirty_;
  dirty_ = false;
  return value;
}

bool RainLockManager::takeLockChanged(bool& lockedValue) {
  if (!lockChangePending_) return false;
  lockChangePending_ = false;
  lockedValue = changedLockValue_;
  return true;
}

bool RainLockManager::contactInTarget(const TouchContact& contact, int16_t targetX) const {
  if (!contact.valid) return false;
  return distanceSquared(contact.x, contact.y, targetX, kTargetY) <=
         static_cast<uint32_t>(kTargetRadius) * kTargetRadius;
}

bool RainLockManager::findInitialTargets(const TouchPoint& frame, const TouchContact*& left,
                                         const TouchContact*& right) const {
  if (frame.points != 2 || !frame.contacts[0].valid || !frame.contacts[1].valid) return false;
  if (frame.contacts[0].id == frame.contacts[1].id) return false;
  if (contactInTarget(frame.contacts[0], kLeftX) &&
      contactInTarget(frame.contacts[1], kRightX)) {
    left = &frame.contacts[0];
    right = &frame.contacts[1];
    return true;
  }
  if (contactInTarget(frame.contacts[1], kLeftX) &&
      contactInTarget(frame.contacts[0], kRightX)) {
    left = &frame.contacts[1];
    right = &frame.contacts[0];
    return true;
  }
  return false;
}

bool RainLockManager::validateTrackedTargets(const TouchPoint& frame,
                                             const TouchContact*& left,
                                             const TouchContact*& right) const {
  if (frame.points != 2 || leftId_ == rightId_) return false;
  left = findById(frame, leftId_);
  right = findById(frame, rightId_);
  if (!left || !right) return false;
  return contactInTarget(*left, kLeftX) && contactInTarget(*right, kRightX) &&
         movementPlausible(*left, lastLeftX_, lastLeftY_) &&
         movementPlausible(*right, lastRightX_, lastRightY_);
}

const TouchContact* RainLockManager::findById(const TouchPoint& frame, uint8_t id) const {
  for (uint8_t i = 0; i < 2; ++i) {
    if (frame.contacts[i].valid && frame.contacts[i].id == id) return &frame.contacts[i];
  }
  return nullptr;
}

bool RainLockManager::movementPlausible(const TouchContact& contact, int16_t lastX,
                                        int16_t lastY) const {
  return distanceSquared(contact.x, contact.y, lastX, lastY) <=
         static_cast<uint32_t>(kMaximumTrackedStepPx) * kMaximumTrackedStepPx;
}

void RainLockManager::showHint(uint32_t nowMs) {
  hintUntilMs_ = nowMs + kHintToastMs;
  dirty_ = true;
}

void RainLockManager::startPriming(const TouchContact& left,
                                   const TouchContact& right,
                                   uint32_t nowMs) {
  state_ = RainLockState::Priming;
  hintUntilMs_ = 0;
  preHoldStartMs_ = nowMs;
  preHoldElapsedMs_ = 0;
  holdStartMs_ = 0;
  holdElapsedMs_ = 0;
  progress_ = 0.0f;
  leftId_ = left.id;
  rightId_ = right.id;
  lastLeftX_ = left.x;
  lastLeftY_ = left.y;
  lastRightX_ = right.x;
  lastRightY_ = right.y;
  dirty_ = true;
}

void RainLockManager::startHolding(const TouchContact& left, const TouchContact& right,
                                   uint32_t nowMs) {
  state_ = RainLockState::Holding;
  preHoldElapsedMs_ = kPreHoldDurationMs;
  holdStartMs_ = nowMs;
  holdElapsedMs_ = 0;
  progress_ = 0;
  leftId_ = left.id;
  rightId_ = right.id;
  lastLeftX_ = left.x;
  lastLeftY_ = left.y;
  lastRightX_ = right.x;
  lastRightY_ = right.y;
  dirty_ = true;
}

void RainLockManager::abortUnlock(uint32_t nowMs) {
  state_ = RainLockState::LockedIdle;
  hintUntilMs_ = nowMs + kHintToastMs;
  clearTracking();
  dirty_ = true;
}

void RainLockManager::clearTracking() {
  preHoldStartMs_ = 0;
  preHoldElapsedMs_ = 0;
  holdStartMs_ = 0;
  holdElapsedMs_ = 0;
  progress_ = 0;
  leftId_ = rightId_ = 0xFF;
}

void RainLockManager::completeUnlock(uint32_t nowMs) {
  state_ = RainLockState::UnlockSuccess;
  successStartedMs_ = nowMs;
  hintUntilMs_ = 0;
  holdElapsedMs_ = kHoldDurationMs;
  progress_ = 1.0f;
  lockChangePending_ = true;
  changedLockValue_ = false;
  dirty_ = true;
}
