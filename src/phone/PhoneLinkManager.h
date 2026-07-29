#pragma once

#include <Arduino.h>

#include "media/MediaState.h"
#include "navigation/NavigationState.h"
#include "phone/BikeProtocol.h"
#include "phone/BleTransport.h"
#include "phone/PhoneState.h"
#include "time/ClockManager.h"

class RideSyncManager;
class ConfigSyncManager;

struct LiveTelemetryInput {
  uint8_t rideState = 0;
  uint8_t motionState = 0;
  uint8_t batteryPercent = 0;
  uint8_t sdState = 0;
  float speedKmh = 0;
  float distanceM = 0;
  float averageSpeedKmh = 0;
  float maxSpeedKmh = 0;
  uint64_t movingTimeMs = 0;
  uint64_t elapsedTimeMs = 0;
  uint32_t pulseCount = 0;
  uint32_t rideId = 0;
  bool rainLocked = false;
  bool batteryLow = false;
};

class PhoneLinkManager {
 public:
  bool begin();
  void update(uint32_t nowMs);
  void startPairing(uint32_t nowMs);
  void cancelPairing();
  void updateLiveData(const LiveTelemetryInput& data, uint32_t nowMs);
  void attachSync(RideSyncManager& sync) { sync_ = &sync; }
  void attachConfig(ConfigSyncManager& config) { config_ = &config; }
  void prepareForUsb();

  const PhoneState& state() const { return state_; }
  bool ready() const {
    return state_.link == PhoneLinkState::Ready ||
           state_.link == PhoneLinkState::Transferring;
  }
  uint64_t deviceId() const { return deviceId_; }
  uint64_t associationId() const { return associationId_; }
  uint16_t negotiatedMtu() const { return transport_.negotiatedMtu(); }
  ClockManager& clock() { return clock_; }
  const ClockManager& clock() const { return clock_; }
  const media::MediaState& mediaState() const { return mediaState_; }
  const navigation::NavigationState& navigationState() const {
    return navigationState_;
  }
  uint32_t mediaRevision() const { return mediaRevision_; }
  uint32_t navigationRevision() const { return navigationRevision_; }
  bool sendMediaAction(media::Action action, uint64_t positionMs = 0);

  bool sendMessage(bikeproto::MessageType type, uint8_t flags,
                   const uint8_t* payload, uint16_t payloadLength,
                   bool indicate = false);
  bool sendResponseMessage(bikeproto::MessageType type,
                           uint16_t requestSequence, const uint8_t* payload,
                           uint16_t payloadLength, bool indicate);
  void sendProtocolError(bikeproto::ErrorCode code,
                         bikeproto::MessageType rejectedType,
                         uint16_t rejectedSequence, const char* detail);

 private:
  void loadIdentity();
  void persistAssociation();
  void createAssociation();
  void resetSession();
  void processIncoming();
  void handleFrame(const bikeproto::Frame& frame);
  void handleHello(const bikeproto::Frame& frame);
  void handleTimeSync(const bikeproto::Frame& frame);
  void handleMediaState(const bikeproto::Frame& frame);
  void handleNavigationState(const bikeproto::Frame& frame);
  void sendHelloAck(uint16_t requestSequence);
  bool sendResponse(bikeproto::MessageType type, uint16_t requestSequence,
                    const uint8_t* payload, uint16_t payloadLength,
                    bool indicate);
  void sendError(bikeproto::ErrorCode code, bikeproto::MessageType rejectedType,
                 uint16_t rejectedSequence, const char* detail);
  void noteError(const char* detail);
  bool helloHasProtocolOne(const uint8_t* payload, uint16_t length,
                           size_t& offset) const;

  BleTransport transport_;
  bikeproto::Decoder decoder_;
  // Protocol frames are intentionally members: keeping these buffers off the
  // Arduino loopTask stack leaves enough headroom for SD/FatFs callbacks.
  uint8_t incomingBytes_[256] = {0};
  bikeproto::Frame incomingFrame_;
  PhoneState state_;
  uint64_t deviceId_ = 0;
  uint64_t associationId_ = 0;
  uint16_t nextTxSequence_ = 1;
  bool lastConnected_ = false;
  bool lastAuthenticated_ = false;
  bool associationCreatedThisSession_ = false;
  ClockManager clock_;
  uint32_t lastTelemetryMs_ = 0;
  bool liveDataInitialized_ = false;
  uint8_t previousRideState_ = 0;
  uint8_t previousSdState_ = 0;
  bool previousRainLocked_ = false;
  bool previousBatteryLow_ = false;
  RideSyncManager* sync_ = nullptr;
  ConfigSyncManager* config_ = nullptr;
  media::MediaState mediaState_;
  navigation::NavigationState navigationState_;
  uint32_t mediaRevision_ = 0;
  uint32_t navigationRevision_ = 0;
};
