#include <Arduino.h>
#include <unity.h>

#include "battery/BatteryMath.h"
#include "config/app_config.h"
#include "rain/RainLockManager.h"
#include "phone/BikeProtocol.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedMath.h"
#include "led/SpeedTrendMath.h"
#include "protocol_vectors.generated.h"

// Host-independent state-machine implementation is compiled into this embedded Unity target.
#include "../../src/speed/RideStateMachine.cpp"
#include "../../src/rain/RainLockManager.cpp"
#include "../../src/phone/BikeProtocol.cpp"

TouchPoint rainFrame(bool left, bool right, bool swapped = false) {
  TouchPoint frame;
  frame.points = static_cast<uint8_t>(left) + static_cast<uint8_t>(right);
  frame.touched = frame.points > 0;
  if (left) {
    TouchContact& c = frame.contacts[swapped && right ? 1 : 0];
    c.valid = true; c.id = 4; c.event = 2; c.x = 160; c.y = 192;
  }
  if (right) {
    TouchContact& c = frame.contacts[swapped ? 0 : (left ? 1 : 0)];
    c.valid = true; c.id = 9; c.event = 2; c.x = 320; c.y = 192;
  }
  return frame;
}

void test_speed_intervals_and_ppr() {
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 7.8984f, speedmath::kmhFromIntervalUs(1000000, 2.194f, 1));
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 3.9492f, speedmath::kmhFromIntervalUs(1000000, 2.194f, 2));
  TEST_ASSERT_EQUAL_FLOAT(0, speedmath::kmhFromIntervalUs(0, 2.194f, 1));
}
void test_settings_validation() {
  app::AppSettings s;
  s.wheelCircumferenceM = 10;
  s.pulsesPerRevolution = 0;
  s.autoPauseDelayMs = 50;
  s.batteryCalibrationFactor = 2;
  s.displayBrightnessPercent = 0;
  s.rgbSpeedTrendToleranceKmh = 9;
  s.rgbSpeedTrendTolerance5sKmh = 9;
  s.rgbSpeedTrendTolerance10sKmh = 9;
  s.rgbLedBrightnessPercent = 0;
  app::validateSettings(s);
  TEST_ASSERT_FLOAT_WITHIN(.001f, 2.194f, s.wheelCircumferenceM);
  TEST_ASSERT_EQUAL_UINT8(1, s.pulsesPerRevolution);
  TEST_ASSERT_EQUAL_UINT32(5000, s.autoPauseDelayMs);
  TEST_ASSERT_FLOAT_WITHIN(.001f, 1, s.batteryCalibrationFactor);
  TEST_ASSERT_EQUAL_UINT8(80, s.displayBrightnessPercent);
  TEST_ASSERT_FLOAT_WITHIN(.001f, .5f, s.rgbSpeedTrendToleranceKmh);
  TEST_ASSERT_FLOAT_WITHIN(.001f, .5f, s.rgbSpeedTrendTolerance5sKmh);
  TEST_ASSERT_FLOAT_WITHIN(.001f, .5f, s.rgbSpeedTrendTolerance10sKmh);
  TEST_ASSERT_EQUAL_UINT8(20, s.rgbLedBrightnessPercent);

  app::AppSettings nonFinite;
  nonFinite.wheelCircumferenceM = NAN;
  nonFinite.stopThresholdKmh = INFINITY;
  nonFinite.maxPlausibleSpeedKmh = -INFINITY;
  nonFinite.rgbSpeedTrendToleranceKmh = NAN;
  nonFinite.batteryCalibrationFactor = NAN;
  app::validateSettings(nonFinite);
  TEST_ASSERT_FLOAT_WITHIN(.001f, 2.194f, nonFinite.wheelCircumferenceM);
  TEST_ASSERT_FLOAT_WITHIN(.001f, 3.0f, nonFinite.stopThresholdKmh);
  TEST_ASSERT_FLOAT_WITHIN(.001f, 100.0f, nonFinite.maxPlausibleSpeedKmh);
  TEST_ASSERT_FLOAT_WITHIN(.001f, .5f,
                           nonFinite.rgbSpeedTrendToleranceKmh);
  TEST_ASSERT_FLOAT_WITHIN(.001f, 1.0f,
                           nonFinite.batteryCalibrationFactor);
}
void test_speed_trend_tolerance() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SpeedTrendState::Accelerating),
                        static_cast<int>(speedtrend::classify(21.0f, 20.0f, 0.5f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SpeedTrendState::Stable),
                        static_cast<int>(speedtrend::classify(20.5f, 20.0f, 0.5f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SpeedTrendState::Stable),
                        static_cast<int>(speedtrend::classify(19.5f, 20.0f, 0.5f)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(SpeedTrendState::Decelerating),
                        static_cast<int>(speedtrend::classify(19.4f, 20.0f, 0.5f)));
}
void test_auto_pause_is_motion_state_not_ride_state() {
  app::AppSettings cfg;
  cfg.autoPauseEnabled = true;
  cfg.autoPauseDelayMs = 5000;
  RideStateMachine r;
  r.begin(&cfg);
  r.start(100, 0);
  r.update(1100, 20, 1);
  r.update(2100, 0, 1);
  r.update(7100, 0, 1);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RideState::RIDING),
                        static_cast<int>(r.state()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(MotionState::AUTO_PAUSED),
                        static_cast<int>(r.motionState()));
  const uint64_t pausedMovingTime = r.stats().movingMs;
  r.update(7200, 20, 2);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(MotionState::MOVING),
                        static_cast<int>(r.motionState()));
  TEST_ASSERT_TRUE(r.stats().movingMs > pausedMovingTime);
}
void test_battery_math() {
  TEST_ASSERT_FLOAT_WITHIN(.001f,4.2f,batterymath::calibratedVoltage(2.1f,2,1));
  TEST_ASSERT_FLOAT_WITHIN(.1f,25,batterymath::percentFromVoltage(3.70f));
  TEST_ASSERT_FLOAT_WITHIN(.1f,100,batterymath::percentFromVoltage(4.25f));
}
void test_ride_pause_and_distance() {
  app::AppSettings cfg; RideStateMachine r; r.begin(&cfg); r.start(100,0); r.update(1100,20,1); TEST_ASSERT_FLOAT_WITHIN(.001f,2.194f,r.stats().distanceM); TEST_ASSERT_EQUAL_UINT32(1000,static_cast<uint32_t>(r.stats().movingMs)); TEST_ASSERT_FLOAT_WITHIN(.01f,7.8984f,r.stats().averageMovingSpeedKmh);
  r.pause(1100); r.update(2100,20,2); TEST_ASSERT_FLOAT_WITHIN(.001f,2.194f,r.stats().distanceM); TEST_ASSERT_EQUAL_UINT32(1000,static_cast<uint32_t>(r.stats().recordingMs)); TEST_ASSERT_EQUAL_UINT32(1000,static_cast<uint32_t>(r.stats().pauseMs));
}
void test_rain_lock_requires_continuous_two_point_hold() {
  RainLockManager lock;
  lock.reset();
  TEST_ASSERT_TRUE(lock.enable(100));
  TouchPoint both = rainFrame(true, true);
  TouchPoint released;
  lock.update(released, 150);  // Release the Rain button activation contact.
  lock.update(released, 1500); // Let the enable toast finish.
  lock.update(both, 1600);     // Start the two-point pre-hold.
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::Priming),
                        static_cast<int>(lock.state()));
  lock.update(both, 3599);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::Priming),
                        static_cast<int>(lock.state()));
  lock.update(released, 3600);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::LockedIdle),
                        static_cast<int>(lock.state()));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lock.progress());

  lock.update(both, 3700);
  lock.update(both, 5700);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::Holding),
                        static_cast<int>(lock.state()));
  lock.update(both, 8699);
  TEST_ASSERT_TRUE(lock.locked());
  lock.update(both, 8700);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::UnlockSuccess),
                        static_cast<int>(lock.state()));
  TEST_ASSERT_FALSE(lock.locked());
}
void test_rain_lock_accepts_swapped_order_and_rejects_wrong_zones() {
  RainLockManager lock;
  lock.reset();
  lock.enable(0);
  TouchPoint released;
  lock.update(released, 5);
  lock.update(released, 1400);
  TouchPoint outside = rainFrame(true, true);
  outside.contacts[0].x = 40;
  outside.contacts[1].x = 440;
  lock.update(outside, 1500);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::LockedIdle),
                        static_cast<int>(lock.state()));
  TEST_ASSERT_TRUE(lock.hintVisible());
  TouchPoint swapped = rainFrame(true, true, true);
  lock.update(swapped, 1600);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::Priming),
                        static_cast<int>(lock.state()));
  swapped.contacts[0].id = 7;
  lock.update(swapped, 1601);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RainLockState::LockedIdle),
                        static_cast<int>(lock.state()));
}
void test_protocol_matches_canonical_vectors() {
  uint8_t encoded[32] = {0};
  uint16_t length = 0;
  TEST_ASSERT_TRUE(bikeproto::Codec::encode(
      bikeproto::MessageType::Hello, 0, 1, nullptr, 0, encoded, sizeof(encoded), length));
  TEST_ASSERT_EQUAL_UINT16(sizeof(protocolvectors::kValidEmptyHello), length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(protocolvectors::kValidEmptyHello, encoded,
                                sizeof(protocolvectors::kValidEmptyHello));

  const uint8_t pingPayload[] = {0x78,0x56,0x34,0x12};
  TEST_ASSERT_TRUE(bikeproto::Codec::encode(
      bikeproto::MessageType::Ping, 0, 42, pingPayload, sizeof(pingPayload),
      encoded, sizeof(encoded), length));
  TEST_ASSERT_EQUAL_UINT8_ARRAY(protocolvectors::kValidPing, encoded,
                                sizeof(protocolvectors::kValidPing));
}
void test_protocol_partial_multiple_and_crc_error() {
  const uint8_t frames[] = {
      0x42,0x43,0x01,0x01,0x00,0x01,0x00,0x00,0x00,0x1c,0x34,
      0x42,0x43,0x01,0x04,0x00,0x2a,0x00,0x04,0x00,0x78,0x56,0x34,0x12,0xf6,0x69};
  bikeproto::Decoder decoder;
  bikeproto::Frame frame;
  decoder.feed(frames, 7);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(bikeproto::DecodeResult::NeedMoreData),
                        static_cast<int>(decoder.next(frame)));
  decoder.feed(frames + 7, sizeof(frames) - 7);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(bikeproto::DecodeResult::FrameReady),
                        static_cast<int>(decoder.next(frame)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(bikeproto::MessageType::Hello),
                          static_cast<uint8_t>(frame.type));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(bikeproto::DecodeResult::FrameReady),
                        static_cast<int>(decoder.next(frame)));
  TEST_ASSERT_EQUAL_UINT16(42, frame.sequence);
  uint8_t bad[sizeof(frames)] = {0};
  memcpy(bad, frames, sizeof(frames));
  bad[10] ^= 1;
  decoder.reset();
  decoder.feed(bad, 11);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(bikeproto::DecodeResult::CrcMismatch),
                        static_cast<int>(decoder.next(frame)));
}
void test_protocol_maximum_payload_vector() {
  uint8_t payload[protocolvectors::kMaximumPayloadBytes];
  uint8_t encoded[protocolvectors::kMaximumFrameBytes];
  for (uint16_t i = 0; i < sizeof(payload); ++i) payload[i] = i & 0xFF;
  uint16_t length = 0;
  TEST_ASSERT_TRUE(bikeproto::Codec::encode(
      bikeproto::MessageType::Ping, 0, 0xFFFF, payload, sizeof(payload),
      encoded, sizeof(encoded), length));
  TEST_ASSERT_EQUAL_UINT16(protocolvectors::kMaximumFrameBytes, length);
  TEST_ASSERT_EQUAL_HEX16(
      protocolvectors::kMaximumPayloadCrc,
      bikeproto::readU16(encoded + length - sizeof(uint16_t)));
}
void setup() {
  Serial.begin(115200);
  // Native ESP32-S3 USB/JTAG can enumerate after the application has started;
  // leave enough time for PlatformIO's test monitor to attach.
  delay(5000);
  UNITY_BEGIN();
  RUN_TEST(test_speed_intervals_and_ppr);
  RUN_TEST(test_settings_validation);
  RUN_TEST(test_speed_trend_tolerance);
  RUN_TEST(test_battery_math);
  RUN_TEST(test_ride_pause_and_distance);
  RUN_TEST(test_auto_pause_is_motion_state_not_ride_state);
  RUN_TEST(test_rain_lock_requires_continuous_two_point_hold);
  RUN_TEST(test_rain_lock_accepts_swapped_order_and_rejects_wrong_zones);
  RUN_TEST(test_protocol_matches_canonical_vectors);
  RUN_TEST(test_protocol_partial_multiple_and_crc_error);
  RUN_TEST(test_protocol_maximum_payload_vector);
  UNITY_END();
}
void loop() {}
