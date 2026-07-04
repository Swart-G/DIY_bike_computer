#include "speed/RideStateMachine.h"

void RideStateMachine::begin(const app::AppSettings* settings) {
  settings_ = settings;
  state_ = RideState::IDLE;
  stats_ = RideStats();
}

void RideStateMachine::update(uint32_t nowMs, float currentSpeedKmh, uint32_t absolutePulseCount) {
  if (lastUpdateMs_ == 0) {
    lastUpdateMs_ = nowMs;
    lastAbsolutePulseCount_ = absolutePulseCount;
    return;
  }

  if (absolutePulseCount < lastAbsolutePulseCount_) {
    lastAbsolutePulseCount_ = absolutePulseCount;
  }

  const uint32_t elapsedDelta = nowMs - lastUpdateMs_;
  const uint32_t pulseDelta = absolutePulseCount - lastAbsolutePulseCount_;

  if (state_ == RideState::RIDING) {
    stats_.movingMs += elapsedDelta;
    stats_.elapsedMs = nowMs - rideStartMs_;
    stats_.distanceM += static_cast<float>(pulseDelta) *
                        (settings_ ? settings_->wheelCircumferenceM : 2.194f);
    stats_.pulseCount += pulseDelta;
    if (currentSpeedKmh > stats_.maxSpeedKmh) {
      stats_.maxSpeedKmh = currentSpeedKmh;
    }
    if (stats_.movingMs > 0) {
      stats_.avgSpeedKmh = (stats_.distanceM / (static_cast<float>(stats_.movingMs) / 1000.0f)) * 3.6f;
    }
  } else if (state_ == RideState::PAUSED || state_ == RideState::FINISHED) {
    stats_.elapsedMs = nowMs - rideStartMs_;
    stats_.pauseMs = stats_.elapsedMs > stats_.movingMs ? stats_.elapsedMs - stats_.movingMs : 0;
  }

  lastAbsolutePulseCount_ = absolutePulseCount;
  lastUpdateMs_ = nowMs;
}

void RideStateMachine::start(uint32_t nowMs, uint32_t absolutePulseCount) {
  stats_ = RideStats();
  state_ = RideState::RIDING;
  rideStartMs_ = nowMs;
  stateEnteredMs_ = nowMs;
  lastUpdateMs_ = nowMs;
  lastAbsolutePulseCount_ = absolutePulseCount;
  lastRecoverySaveMs_ = 0;
}

void RideStateMachine::pause(uint32_t nowMs) {
  if (state_ != RideState::RIDING) {
    return;
  }
  state_ = RideState::PAUSED;
  stateEnteredMs_ = nowMs;
}

void RideStateMachine::resume(uint32_t nowMs) {
  if (state_ != RideState::PAUSED) {
    return;
  }
  state_ = RideState::RIDING;
  stateEnteredMs_ = nowMs;
}

void RideStateMachine::finish(uint32_t nowMs) {
  if (state_ != RideState::RIDING && state_ != RideState::PAUSED) {
    return;
  }
  state_ = RideState::FINISHED;
  stateEnteredMs_ = nowMs;
  stats_.elapsedMs = nowMs - rideStartMs_;
  stats_.pauseMs = stats_.elapsedMs > stats_.movingMs ? stats_.elapsedMs - stats_.movingMs : 0;
}

void RideStateMachine::newRide(uint32_t nowMs, uint32_t absolutePulseCount) {
  stats_ = RideStats();
  state_ = RideState::IDLE;
  rideStartMs_ = nowMs;
  stateEnteredMs_ = nowMs;
  lastUpdateMs_ = nowMs;
  lastAbsolutePulseCount_ = absolutePulseCount;
  lastRecoverySaveMs_ = 0;
}

void RideStateMachine::restorePaused(const RideRecoveryData& recovery, uint32_t nowMs,
                                     uint32_t absolutePulseCount) {
  stats_ = recovery.stats;
  state_ = RideState::PAUSED;
  rideStartMs_ = nowMs - stats_.elapsedMs;
  stateEnteredMs_ = nowMs;
  lastUpdateMs_ = nowMs;
  lastAbsolutePulseCount_ = absolutePulseCount;
  lastRecoverySaveMs_ = 0;
}

String RideStateMachine::stateText() const {
  switch (state_) {
    case RideState::RIDING:
      return "RIDING";
    case RideState::PAUSED:
      return "PAUSED";
    case RideState::FINISHED:
      return "FINISHED";
    case RideState::IDLE:
    default:
      return "IDLE";
  }
}

bool RideStateMachine::needsRecoverySave(uint32_t nowMs, uint32_t intervalMs) const {
  return (state_ == RideState::RIDING || state_ == RideState::PAUSED) &&
         (lastRecoverySaveMs_ == 0 || nowMs - lastRecoverySaveMs_ >= intervalMs);
}

RideRecoveryData RideStateMachine::recoveryData() const {
  RideRecoveryData data;
  data.valid = state_ == RideState::RIDING || state_ == RideState::PAUSED;
  data.lastState = state_;
  data.stats = stats_;
  return data;
}

void RideStateMachine::markRecoverySaved(uint32_t nowMs) {
  lastRecoverySaveMs_ = nowMs;
}
