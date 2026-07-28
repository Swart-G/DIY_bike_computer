#include "speed/RideStateMachine.h"

#include <cstring>

void RideStateMachine::begin(const app::AppSettings* settings) { settings_ = settings; newRide(millis(), 0); }

void RideStateMachine::update(uint32_t nowMs, float speed, uint32_t absolutePulses, uint32_t rejectedPulses) {
  if (!lastUpdateMs_) { lastUpdateMs_ = nowMs; lastAbsolutePulseCount_ = absolutePulses; return; }
  const uint32_t dt = nowMs - lastUpdateMs_;
  if (absolutePulses < lastAbsolutePulseCount_) lastAbsolutePulseCount_ = absolutePulses;
  const uint32_t pulses = absolutePulses - lastAbsolutePulseCount_;
  if (state_ == RideState::RIDING || state_ == RideState::PAUSED) stats_.elapsedMs += dt;
  if (state_ == RideState::RIDING) {
    stats_.recordingMs += dt;
    const float threshold = settings_ ? settings_->stopThresholdKmh : 3.0f;
    moving_ = moving_ ? speed >= threshold * 0.75f : speed >= threshold;
    if (moving_) stats_.movingMs += dt;
    stats_.distanceM += static_cast<float>(pulses) * (settings_ ? settings_->wheelCircumferenceM / settings_->pulsesPerRevolution : 2.194f);
    stats_.pulseCount += pulses;
    if (speed > stats_.maxSpeedKmh) stats_.maxSpeedKmh = speed;
  }
  stats_.pauseMs = stats_.elapsedMs > stats_.recordingMs ? stats_.elapsedMs - stats_.recordingMs : 0;
  stats_.stoppedMs = stats_.recordingMs > stats_.movingMs ? stats_.recordingMs - stats_.movingMs : 0;
  stats_.rejectedPulseCount = rejectedPulses;
  updateAverages();
  lastAbsolutePulseCount_ = absolutePulses;
  lastUpdateMs_ = nowMs;
}
void RideStateMachine::updateAverages() {
  // distance is metres and time is milliseconds: m * 3600 / ms = km/h.
  stats_.averageMovingSpeedKmh = stats_.movingMs ? stats_.distanceM * 3600.0f / stats_.movingMs : 0;
  stats_.averageRecordingSpeedKmh = stats_.recordingMs ? stats_.distanceM * 3600.0f / stats_.recordingMs : 0;
}
void RideStateMachine::start(uint32_t nowMs, uint32_t absolute) {
  stats_ = RideStats(); state_ = RideState::RIDING; lastUpdateMs_ = nowMs; lastAbsolutePulseCount_ = absolute;
  lastRecoverySaveMs_ = 0; moving_ = false; rideId_ = 0; rideFolder_[0] = 0; lastSavedSampleIndex_ = 0; loggingGap_ = false; batteryStartVoltage_=batteryMinVoltage_=batteryMaxVoltage_=0;
}
void RideStateMachine::pause(uint32_t nowMs) { if (state_ == RideState::RIDING) { update(nowMs, 0, lastAbsolutePulseCount_, stats_.rejectedPulseCount); state_ = RideState::PAUSED; moving_ = false; } }
void RideStateMachine::resume(uint32_t nowMs) { if (state_ == RideState::PAUSED) { state_ = RideState::RIDING; lastUpdateMs_ = nowMs; moving_ = false; } }
void RideStateMachine::finish(uint32_t nowMs) { if (state_ == RideState::RIDING || state_ == RideState::PAUSED) { update(nowMs, 0, lastAbsolutePulseCount_, stats_.rejectedPulseCount); state_ = RideState::FINISHED; } }
void RideStateMachine::newRide(uint32_t nowMs, uint32_t absolute) {
  stats_ = RideStats(); state_ = RideState::IDLE; lastUpdateMs_ = nowMs; lastAbsolutePulseCount_ = absolute; lastRecoverySaveMs_ = 0;
  moving_ = false; rideId_ = 0; rideFolder_[0] = 0; lastSavedSampleIndex_ = 0; loggingGap_ = false; batteryStartVoltage_=batteryMinVoltage_=batteryMaxVoltage_=0;
}
void RideStateMachine::restorePaused(const RideRecoveryData& r, uint32_t nowMs, uint32_t absolute) {
  stats_ = r.stats; state_ = RideState::PAUSED; lastUpdateMs_ = nowMs; lastAbsolutePulseCount_ = absolute; lastRecoverySaveMs_ = 0;
  moving_ = false; rideId_ = r.rideId; strncpy(rideFolder_, r.rideFolder, sizeof(rideFolder_) - 1); rideFolder_[sizeof(rideFolder_)-1] = 0;
  lastSavedSampleIndex_ = r.lastSavedSampleIndex; loggingGap_ = r.loggingGap; batteryStartVoltage_=r.batteryStartVoltage; batteryMinVoltage_=r.batteryMinVoltage; batteryMaxVoltage_=r.batteryMaxVoltage;
}
String RideStateMachine::stateText() const { switch (state_) { case RideState::RIDING: return "RIDING"; case RideState::PAUSED: return "PAUSED"; case RideState::FINISHED: return "FINISHED"; default: return "IDLE"; } }
bool RideStateMachine::needsRecoverySave(uint32_t nowMs, uint32_t interval) const { return (state_ == RideState::RIDING || state_ == RideState::PAUSED) && (!lastRecoverySaveMs_ || nowMs - lastRecoverySaveMs_ >= interval); }
RideRecoveryData RideStateMachine::recoveryData() const { RideRecoveryData r; r.valid = state_ == RideState::RIDING || state_ == RideState::PAUSED; r.rideId = rideId_; strncpy(r.rideFolder, rideFolder_, sizeof(r.rideFolder)-1); r.lastSavedSampleIndex = lastSavedSampleIndex_; r.loggingGap = loggingGap_; r.batteryStartVoltage=batteryStartVoltage_; r.batteryMinVoltage=batteryMinVoltage_; r.batteryMaxVoltage=batteryMaxVoltage_; r.stats = stats_; r.lastState = state_; return r; }
void RideStateMachine::setRecoveryIdentity(uint32_t id, const char* folder, uint32_t index, bool gap) { rideId_ = id; strncpy(rideFolder_, folder ? folder : "", sizeof(rideFolder_)-1); rideFolder_[sizeof(rideFolder_)-1] = 0; lastSavedSampleIndex_ = index; loggingGap_ = gap; }
void RideStateMachine::setRecoveryBattery(float start, float minimum, float maximum) { batteryStartVoltage_=start; batteryMinVoltage_=minimum; batteryMaxVoltage_=maximum; }
void RideStateMachine::markRecoverySaved(uint32_t nowMs) { lastRecoverySaveMs_ = nowMs; }
