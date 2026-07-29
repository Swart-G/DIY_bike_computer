#include "config/ConfigSyncManager.h"

#include <math.h>

#include "phone/PhoneLinkManager.h"
#include "speed/HallSensor.h"
#include "storage/StorageManager.h"

void ConfigSyncManager::begin(StorageManager& storage, HallSensor& sensor,
                              RideStateMachine& ride,
                              app::AppSettings& settings) {
  storage_ = &storage;
  sensor_ = &sensor;
  settings_ = &settings;
  ride_ = &ride;
}

bool ConfigSyncManager::handleFrame(const bikeproto::Frame& frame,
                                    PhoneLinkManager& link) {
  if (frame.type != bikeproto::MessageType::ConfigGet &&
      frame.type != bikeproto::MessageType::ConfigSet) {
    return false;
  }
  if (!link.state().authorized) {
    link.sendProtocolError(bikeproto::ErrorCode::NotAuthorized, frame.type,
                           frame.sequence, "authorized HELLO required");
    return true;
  }
  if (!settings_ || !storage_ || !sensor_) {
    link.sendProtocolError(bikeproto::ErrorCode::InvalidState, frame.type,
                           frame.sequence, "config manager unavailable");
    return true;
  }
  if (frame.type == bikeproto::MessageType::ConfigGet) {
    if (frame.payloadLength != 2) {
      link.sendProtocolError(bikeproto::ErrorCode::MalformedFrame, frame.type,
                             frame.sequence, "CONFIG_GET requires one key");
      return true;
    }
    sendValue(bikeproto::readU16(frame.payload), frame.sequence, link);
    return true;
  }
  if (frame.payloadLength != 7) {
    link.sendProtocolError(bikeproto::ErrorCode::MalformedFrame, frame.type,
                           frame.sequence, "invalid CONFIG_SET");
    return true;
  }
  if (ride_ && (ride_->state() == RideState::RIDING ||
                ride_->state() == RideState::PAUSED)) {
    link.sendProtocolError(bikeproto::ErrorCode::InvalidState, frame.type,
                           frame.sequence, "finish ride before changing config");
    return true;
  }
  const uint16_t key = bikeproto::readU16(frame.payload);
  const ValueType type = static_cast<ValueType>(frame.payload[2]);
  const Result result = setValue(key, type, frame.payload + 3);
  sendResult(key, result, frame.sequence, link);
  if (result == Result::Ok) sendValue(key, frame.sequence, link);
  return true;
}

void ConfigSyncManager::sendValue(uint16_t key, uint16_t requestSequence,
                                  PhoneLinkManager& link) {
  uint8_t payload[7] = {0};
  bikeproto::writeU16(payload, key);
  switch (key) {
    case WheelCircumference:
      payload[2] = static_cast<uint8_t>(ValueType::Float32);
      bikeproto::writeFloat(payload + 3, settings_->wheelCircumferenceM);
      break;
    case StopThreshold:
      payload[2] = static_cast<uint8_t>(ValueType::Float32);
      bikeproto::writeFloat(payload + 3, settings_->stopThresholdKmh);
      break;
    case AutoPauseEnabled:
      payload[2] = static_cast<uint8_t>(ValueType::Boolean);
      payload[3] = settings_->autoPauseEnabled ? 1 : 0;
      break;
    case AutoPauseDelay:
      payload[2] = static_cast<uint8_t>(ValueType::U32);
      bikeproto::writeU32(payload + 3, settings_->autoPauseDelayMs);
      break;
    case LogInterval:
      payload[2] = static_cast<uint8_t>(ValueType::U32);
      bikeproto::writeU32(payload + 3, settings_->logSampleIntervalMs);
      break;
    case GraphWindow:
      payload[2] = static_cast<uint8_t>(ValueType::U32);
      bikeproto::writeU32(payload + 3, settings_->graphWindowSeconds);
      break;
    default:
      sendResult(key, Result::UnknownKey, requestSequence, link);
      return;
  }
  link.sendResponseMessage(bikeproto::MessageType::ConfigValue,
                           requestSequence, payload, sizeof(payload), false);
}

void ConfigSyncManager::sendResult(uint16_t key, Result result,
                                   uint16_t requestSequence,
                                   PhoneLinkManager& link) {
  uint8_t payload[3] = {0};
  bikeproto::writeU16(payload, key);
  payload[2] = static_cast<uint8_t>(result);
  link.sendResponseMessage(bikeproto::MessageType::ConfigResult,
                           requestSequence, payload, sizeof(payload), false);
}

ConfigSyncManager::Result ConfigSyncManager::setValue(
    uint16_t key, ValueType type, const uint8_t* value) {
  if (storage_->usbModeActive()) return Result::StorageBusyUsb;
  app::AppSettings next = *settings_;
  uint32_t raw = bikeproto::readU32(value);
  float number = 0;
  memcpy(&number, &raw, sizeof(number));
  switch (key) {
    case WheelCircumference:
      if (type != ValueType::Float32) return Result::WrongType;
      if (!isfinite(number) || number < 0.5f || number > 3.5f)
        return Result::InvalidValue;
      next.wheelCircumferenceM = number;
      break;
    case StopThreshold:
      if (type != ValueType::Float32) return Result::WrongType;
      if (!isfinite(number) || number < 0.5f || number > 15.0f)
        return Result::InvalidValue;
      next.stopThresholdKmh = number;
      break;
    case AutoPauseEnabled:
      if (type != ValueType::Boolean) return Result::WrongType;
      if (value[0] > 1 || value[1] || value[2] || value[3])
        return Result::InvalidValue;
      next.autoPauseEnabled = value[0] != 0;
      break;
    case AutoPauseDelay:
      if (type != ValueType::U32) return Result::WrongType;
      if (raw < 1000 || raw > 60000) return Result::InvalidValue;
      next.autoPauseDelayMs = raw;
      break;
    case LogInterval:
      if (type != ValueType::U32) return Result::WrongType;
      if (raw < 250 || raw > 10000) return Result::InvalidValue;
      next.logSampleIntervalMs = raw;
      break;
    case GraphWindow:
      if (type != ValueType::U32) return Result::WrongType;
      if (raw < 10 || raw > 300) return Result::InvalidValue;
      next.graphWindowSeconds = raw;
      break;
    default:
      return Result::UnknownKey;
  }

  String error;
  if (!storage_->saveSettings(next, error)) return Result::StorageUnavailable;
  *settings_ = next;
  if (key == WheelCircumference || key == StopThreshold) {
    sensor_->updateSettings(*settings_);
  }
  ++revision_;
  Serial.printf("[PROTO] config key %u updated\n", key);
  return Result::Ok;
}
