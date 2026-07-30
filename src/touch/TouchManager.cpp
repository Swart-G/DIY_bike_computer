#include "touch/TouchManager.h"

#include "config/hardware_config.h"

namespace {

constexpr uint32_t kTouchReadIntervalMs = 8;
constexpr uint32_t kReleaseGraceMs = 38;
constexpr uint32_t kRecoveryIntervalMs = 1000;

}

bool TouchManager::begin() {
  pinMode(hw::PIN_CTP_INT, INPUT_PULLUP);
  pinMode(hw::PIN_CTP_RST, OUTPUT);

  digitalWrite(hw::PIN_CTP_RST, LOW);
  delay(10);
  digitalWrite(hw::PIN_CTP_RST, HIGH);
  // The FT6336 datasheet allows up to 300 ms after reset before point
  // reporting starts. Probing earlier can leave some panels half-initialised.
  delay(320);

  Wire.begin(hw::PIN_CTP_SDA, hw::PIN_CTP_SCL);
  Wire.setClock(hw::TOUCH_I2C_FREQUENCY_HZ);
  Wire.setTimeOut(20);

  Wire.beginTransmission(hw::FT6336_I2C_ADDRESS);
  ready_ = Wire.endTransmission() == 0;
  if (ready_) {
    writeByte(0x00, 0x00);
  }
  return ready_;
}

bool TouchManager::update() {
  point_.intLevel = digitalRead(hw::PIN_CTP_INT);
  if (!ready_) {
    const uint32_t nowMs = millis();
    releaseIfExpired(nowMs);
    if (nowMs - lastRecoveryAttemptMs_ >= kRecoveryIntervalMs) {
      lastRecoveryAttemptMs_ = nowMs;
      Wire.beginTransmission(hw::FT6336_I2C_ADDRESS);
      if (Wire.endTransmission() == 0) {
        ready_ = true;
        consecutiveReadFailures_ = 0;
        writeByte(0x00, 0x00);
        Serial.println("[TOUCH] FT6336 communication restored");
      }
    }
    return false;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastReadAttemptMs_ < kTouchReadIntervalMs) {
    return true;
  }
  lastReadAttemptMs_ = nowMs;

  // TD_STATUS + P1 (0x03..0x08) + P2 XY (0x09..0x0C). One I2C transaction
  // guarantees both contacts come from the same FT6336 scan.
  uint8_t data[11] = {0};
  if (!readBytes(0x02, data, sizeof(data))) {
    ++consecutiveReadFailures_;
    if (consecutiveReadFailures_ >= 3) {
      ready_ = false;
    }
    releaseIfExpired(nowMs);
    return false;
  }
  consecutiveReadFailures_ = 0;

  const uint8_t reportedPoints = min<uint8_t>(data[0] & 0x0F, 2);
  TouchContact contacts[2];
  bool anyValid = false;
  for (uint8_t i = 0; i < reportedPoints; ++i) {
    const uint8_t base = i == 0 ? 1 : 7;
    TouchContact& contact = contacts[i];
    contact.event = (data[base] >> 6) & 0x03;
    contact.id = (data[base + 2] >> 4) & 0x0F;
    contact.rawX =
        static_cast<int16_t>(((data[base] & 0x0F) << 8) | data[base + 1]);
    contact.rawY =
        static_cast<int16_t>(((data[base + 2] & 0x0F) << 8) | data[base + 3]);
    contact.valid = contact.event == 0 || contact.event == 2;
    if (contact.valid) {
      transformRaw(contact);
      anyValid = true;
    }
  }

  if (!anyValid) {
    releaseIfExpired(nowMs);
    return true;
  }

  point_.points = reportedPoints;
  point_.contacts[0] = contacts[0];
  point_.contacts[1] = contacts[1];
  point_.touched = true;
  const TouchContact& primary =
      point_.contacts[0].valid ? point_.contacts[0] : point_.contacts[1];
  point_.rawX = primary.rawX;
  point_.rawY = primary.rawY;
  point_.x = primary.x;
  point_.y = primary.y;
  point_.lastTouchMs = nowMs;
  lastValidTouchMs_ = nowMs;
  return true;
}

bool TouchManager::readBytes(uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(hw::FT6336_I2C_ADDRESS);
  Wire.write(reg);
  // A STOP and a short bus-free interval stay inside the conservative timing
  // path from the FT6336 datasheet and are more tolerant of long FPC wiring.
  if (Wire.endTransmission(true) != 0) {
    return false;
  }
  delayMicroseconds(6);
  const uint8_t received = Wire.requestFrom(hw::FT6336_I2C_ADDRESS, static_cast<uint8_t>(len));
  if (received != len) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    data[i] = Wire.read();
  }
  return true;
}

bool TouchManager::writeByte(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(hw::FT6336_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void TouchManager::transformRaw(TouchContact& contact) {
  int16_t tx = contact.rawX;
  int16_t ty = contact.rawY;
  if (hw::TOUCH_SWAP_XY) {
    const int16_t tmp = tx;
    tx = ty;
    ty = tmp;
  }
  if (hw::TOUCH_INVERT_X) {
    tx = hw::DISPLAY_WIDTH - 1 - tx;
  }
  if (hw::TOUCH_INVERT_Y) {
    ty = hw::DISPLAY_HEIGHT - 1 - ty;
  }
  contact.x = constrain(tx, 0, hw::DISPLAY_WIDTH - 1);
  contact.y = constrain(ty, 0, hw::DISPLAY_HEIGHT - 1);
}

void TouchManager::clearContacts() {
  point_.contacts[0] = TouchContact();
  point_.contacts[1] = TouchContact();
}

void TouchManager::releaseIfExpired(uint32_t nowMs, bool force) {
  if (!force && point_.touched &&
      nowMs - lastValidTouchMs_ <= kReleaseGraceMs) {
    return;
  }
  point_.touched = false;
  point_.points = 0;
  clearContacts();
}
