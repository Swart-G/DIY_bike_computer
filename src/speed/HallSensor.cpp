#include "speed/HallSensor.h"

#include <esp_timer.h>
#include "config/hardware_config.h"

HallSensor* HallSensor::instance_ = nullptr;

bool HallSensor::begin(const app::AppSettings& settings) {
  instance_ = this;
  updateSettings(settings);
  pinMode(hw::PIN_HALL_SENSOR, pullupEnabled_ ? INPUT_PULLUP : INPUT);
  attachInterrupt(digitalPinToInterrupt(hw::PIN_HALL_SENSOR), HallSensor::isrThunk, interruptMode_);
  started_ = true;
  return true;
}

void HallSensor::updateSettings(const app::AppSettings& settings) {
  minPulseIntervalUs_ = static_cast<uint64_t>(settings.minPulseIntervalMs) * 1000ULL;
  const float metersPerPulse = settings.wheelCircumferenceM / settings.pulsesPerRevolution;
  maxPulseIntervalUs_ = static_cast<uint64_t>((metersPerPulse * 3.6f / settings.maxPlausibleSpeedKmh) * 1000000.0f);
  pullupEnabled_ = settings.sensorPullupEnabled;
  interruptMode_ = settings.sensorInterruptMode;
  if (started_) {
    detachInterrupt(digitalPinToInterrupt(hw::PIN_HALL_SENSOR));
    pinMode(hw::PIN_HALL_SENSOR, pullupEnabled_ ? INPUT_PULLUP : INPUT);
    attachInterrupt(digitalPinToInterrupt(hw::PIN_HALL_SENSOR), HallSensor::isrThunk, interruptMode_);
  }
}

HallSensorSnapshot HallSensor::snapshot() const {
  HallSensorSnapshot result;
  noInterrupts();
  result.pulseCount = pulseCount_;
  result.rejectedPulseCount = rejectedPulseCount_;
  result.lastPulseUs = lastPulseUs_;
  result.lastIntervalUs = lastIntervalUs_;
  interrupts();
  result.pinLevel = digitalRead(hw::PIN_HALL_SENSOR);
  return result;
}

void IRAM_ATTR HallSensor::isrThunk() {
  if (instance_) {
    instance_->handleInterrupt();
  }
}

void IRAM_ATTR HallSensor::handleInterrupt() {
  const uint64_t nowUs = static_cast<uint64_t>(esp_timer_get_time());
  const uint64_t deltaUs = nowUs - lastPulseUs_;
  if (lastPulseUs_ != 0 && (deltaUs < minPulseIntervalUs_ || (maxPulseIntervalUs_ && deltaUs < maxPulseIntervalUs_))) {
    ++rejectedPulseCount_;
    return;
  }
  if (lastPulseUs_ != 0) {
    lastIntervalUs_ = deltaUs;
  }
  lastPulseUs_ = nowUs;
  ++pulseCount_;
}
