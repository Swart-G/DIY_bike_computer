#include "touch/TouchManager.h"

#include "config/hardware_config.h"

bool TouchManager::begin() {
  pinMode(hw::PIN_CTP_INT, INPUT_PULLUP);
  pinMode(hw::PIN_CTP_RST, OUTPUT);

  digitalWrite(hw::PIN_CTP_RST, LOW);
  delay(10);
  digitalWrite(hw::PIN_CTP_RST, HIGH);
  delay(80);

  Wire.begin(hw::PIN_CTP_SDA, hw::PIN_CTP_SCL);
  Wire.setClock(hw::TOUCH_I2C_FREQUENCY_HZ);

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
    point_.touched = false;
    point_.points = 0;
    return false;
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastReadAttemptMs_ < 20) {
    return true;
  }
  lastReadAttemptMs_ = nowMs;

  uint8_t data[5] = {0};
  if (!readBytes(0x02, data, sizeof(data))) {
    ++consecutiveReadFailures_;
    if (consecutiveReadFailures_ >= 3) {
      ready_ = false;
    }
    point_.touched = false;
    point_.points = 0;
    return false;
  }
  consecutiveReadFailures_ = 0;

  point_.points = data[0] & 0x0F;
  point_.touched = point_.points > 0 && point_.points < 3;
  if (!point_.touched) {
    return true;
  }

  point_.rawX = static_cast<int16_t>(((data[1] & 0x0F) << 8) | data[2]);
  point_.rawY = static_cast<int16_t>(((data[3] & 0x0F) << 8) | data[4]);
  transformRaw();
  point_.lastTouchMs = millis();
  return true;
}

bool TouchManager::readBytes(uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(hw::FT6336_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
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

void TouchManager::transformRaw() {
  int16_t tx = point_.rawX;
  int16_t ty = point_.rawY;
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
  point_.x = constrain(tx, 0, hw::DISPLAY_WIDTH - 1);
  point_.y = constrain(ty, 0, hw::DISPLAY_HEIGHT - 1);
}
