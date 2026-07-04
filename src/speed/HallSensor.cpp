#include "speed/HallSensor.h"

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
  minPulseIntervalUs_ = settings.minPulseIntervalMs * 1000UL;
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
  result.lastPulseMs = lastPulseUs_ / 1000UL;
  result.lastIntervalMs = lastIntervalUs_ / 1000UL;
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
  const uint32_t nowUs = micros();
  const uint32_t deltaUs = nowUs - lastPulseUs_;
  if (lastPulseUs_ != 0 && deltaUs < minPulseIntervalUs_) {
    ++rejectedPulseCount_;
    return;
  }
  if (lastPulseUs_ != 0) {
    lastIntervalUs_ = deltaUs;
  }
  lastPulseUs_ = nowUs;
  ++pulseCount_;
}
