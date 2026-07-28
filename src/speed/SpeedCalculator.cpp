#include "speed/SpeedCalculator.h"
#include <esp_timer.h>
#include "speed/SpeedMath.h"
void SpeedCalculator::reset() { rawKmh_ = filteredKmh_ = displayKmh_ = 0; lastPulseCount_ = 0; lastPulseUs_ = lastIntervalUs_ = 0; hasValidInterval_ = false; }
float SpeedCalculator::speedFromInterval(uint64_t us, const app::AppSettings& s) const { return speedmath::kmhFromIntervalUs(us, s.wheelCircumferenceM, s.pulsesPerRevolution); }
void SpeedCalculator::update(const HallSensorSnapshot& sensor, const app::AppSettings& s, uint32_t nowMs) {
  (void)nowMs;
  if (sensor.pulseCount != lastPulseCount_) {
    lastPulseCount_ = sensor.pulseCount; lastPulseUs_ = sensor.lastPulseUs;
    if (sensor.lastIntervalUs) { lastIntervalUs_ = sensor.lastIntervalUs; rawKmh_ = speedFromInterval(lastIntervalUs_, s); filteredKmh_ = filteredKmh_ == 0 ? rawKmh_ : filteredKmh_ * .65f + rawKmh_ * .35f; displayKmh_ = filteredKmh_; hasValidInterval_ = true; }
    return;
  }
  if (!hasValidInterval_ || !lastPulseUs_) { rawKmh_ = filteredKmh_ = displayKmh_ = 0; return; }
  const uint64_t age = timeSincePulseUs();
  if (age <= lastIntervalUs_) { displayKmh_ = filteredKmh_; return; }
  rawKmh_ = speedFromInterval(age, s);
  if (rawKmh_ < s.stopThresholdKmh) { displayKmh_ = filteredKmh_ = 0; } else { filteredKmh_ = filteredKmh_ * .80f + rawKmh_ * .20f; displayKmh_ = filteredKmh_; }
}
uint64_t SpeedCalculator::timeSincePulseUs() const { return lastPulseUs_ ? static_cast<uint64_t>(esp_timer_get_time()) - lastPulseUs_ : 0; }
