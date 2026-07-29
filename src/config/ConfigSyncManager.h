#pragma once

#include <Arduino.h>

#include "config/app_config.h"
#include "phone/BikeProtocol.h"

class HallSensor;
class PhoneLinkManager;
class StorageManager;
class RideStateMachine;

class ConfigSyncManager {
 public:
  void begin(StorageManager& storage, HallSensor& sensor,
             RideStateMachine& ride, app::AppSettings& settings);
  bool handleFrame(const bikeproto::Frame& frame, PhoneLinkManager& link);
  uint32_t revision() const { return revision_; }

 private:
  enum class ValueType : uint8_t { Boolean = 1, U32 = 2, Float32 = 3 };
  enum class Result : uint8_t {
    Ok = 0,
    UnknownKey = 1,
    WrongType = 2,
    InvalidValue = 3,
    StorageUnavailable = 4,
    StorageBusyUsb = 5,
  };
  enum Key : uint16_t {
    WheelCircumference = 1,
    StopThreshold = 2,
    AutoPauseEnabled = 3,
    AutoPauseDelay = 4,
    LogInterval = 5,
    GraphWindow = 6,
  };

  void sendValue(uint16_t key, uint16_t requestSequence,
                 PhoneLinkManager& link);
  void sendResult(uint16_t key, Result result, uint16_t requestSequence,
                  PhoneLinkManager& link);
  Result setValue(uint16_t key, ValueType type, const uint8_t* value);

  StorageManager* storage_ = nullptr;
  HallSensor* sensor_ = nullptr;
  app::AppSettings* settings_ = nullptr;
  RideStateMachine* ride_ = nullptr;
  uint32_t revision_ = 0;
};
