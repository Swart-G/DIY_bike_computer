#include "phone/PhoneLinkManager.h"

#include <algorithm>
#include <Preferences.h>
#include <esp_system.h>

#include "config/app_config.h"
#include "config/ConfigSyncManager.h"
#include "sync/RideSyncManager.h"

namespace {

constexpr char kDisplayName[] = "DIY Bike Computer";

uint64_t randomU64() {
  uint64_t value = (static_cast<uint64_t>(esp_random()) << 32) | esp_random();
  return value ? value : 1;
}

}  // namespace

bool PhoneLinkManager::begin() {
  loadIdentity();
  uint8_t deviceInfo[18] = {0};
  deviceInfo[0] = bikeproto::kProtocolVersion;
  deviceInfo[1] = bikeproto::kProtocolVersion;
  deviceInfo[2] = 2;
  deviceInfo[3] = 2;
  deviceInfo[4] = 0;
  deviceInfo[5] = 0;  // release build
  bikeproto::writeU32(deviceInfo + 6, bikeproto::kImplementedCapabilities);
  bikeproto::writeU64(deviceInfo + 10, deviceId_);

  const bool ok =
      transport_.begin(kDisplayName, deviceInfo, sizeof(deviceInfo));
  if (ok && rememberedPhoneCount_ && !transport_.hasStoredBond()) {
    rememberedPhoneCount_ = 0;
    memset(rememberedPhones_, 0, sizeof(rememberedPhones_));
    persistAssociations();
    Serial.println("[BLE] cleared stale phone list without controller bonds");
  }
  transport_.setKnownAssociation(rememberedPhoneCount_ != 0);
  state_.paired = rememberedPhoneCount_ != 0;
  state_.capabilities = bikeproto::kImplementedCapabilities;
  strlcpy(state_.displayName, kDisplayName, sizeof(state_.displayName));
  state_.link = ok ? PhoneLinkState::Advertising : PhoneLinkState::Error;
  if (!ok) noteError("BLE initialization failed");
  return ok;
}

void PhoneLinkManager::update(uint32_t nowMs) {
  transport_.update(nowMs);
  state_.negotiatedMtu = transport_.negotiatedMtu();
  state_.pairingCode =
      transport_.pairingActive(nowMs) ? transport_.pairingCode() : 0;
  state_.pairingExpiresMs =
      state_.pairingCode ? transport_.pairingExpiresMs() : 0;

  // Keep the application session inactive until the disconnect callback has
  // run and the controller bond has been removed.
  if (transport_.cancellingPairing()) {
    state_.paired = false;
    state_.link = PhoneLinkState::Advertising;
    return;
  }

  const bool connected = transport_.connected();
  const bool authenticated = transport_.authenticated();
  if (connected != lastConnected_) {
    lastConnected_ = connected;
    if (connected) {
      resetSession();
      state_.link = PhoneLinkState::Connected;
      state_.connectedSinceMs = nowMs;
      Serial.println("[BLE] central connected");
    } else {
      resetSession();
      state_.link = PhoneLinkState::Advertising;
      state_.connectedSinceMs = 0;
      Serial.println("[BLE] central disconnected");
    }
  }

  if (authenticated != lastAuthenticated_) {
    lastAuthenticated_ = authenticated;
    if (authenticated) {
      state_.link = PhoneLinkState::Authenticating;
      if (transport_.pairingActive(nowMs)) {
        createAssociation();
        associationCreatedThisSession_ = true;
      }
    } else if (connected) {
      state_.link = PhoneLinkState::Authenticating;
    }
  }

  if (!connected) return;
  if (transport_.rxOverflowed()) {
    decoder_.reset();
    noteError("BLE RX overflow");
    sendError(bikeproto::ErrorCode::MalformedFrame,
              bikeproto::MessageType::Error, 0, "RX overflow");
  }
  processIncoming();
  if (sync_) {
    sync_->update(*this, nowMs);
    const RideSyncState syncState = sync_->state();
    if (state_.protocolVersion && state_.authorized &&
        (syncState == RideSyncState::BuildingManifests ||
         syncState == RideSyncState::SendingChunk ||
         syncState == RideSyncState::WaitingAck)) {
      state_.link = PhoneLinkState::Transferring;
    } else if (state_.protocolVersion && state_.authorized) {
      state_.link = PhoneLinkState::Ready;
    }
    if (sync_->lastCompletedMs()) state_.lastSyncMs = sync_->lastCompletedMs();
  }
}

void PhoneLinkManager::startPairing(uint32_t nowMs) {
  if (transport_.cancellingPairing()) return;
  if (!canRememberAnotherPhone()) {
    noteError("Remembered phone list is full");
    return;
  }
  associationId_ = 0;
  associationCreatedThisSession_ = false;
  transport_.startPairing(nowMs);
  state_.pairingCode = transport_.pairingCode();
  state_.pairingExpiresMs = transport_.pairingExpiresMs();
}

void PhoneLinkManager::cancelPairing() {
  if (sync_) sync_->cancel();
  transport_.cancelPairing(false);
  associationId_ = 0;
  transport_.setKnownAssociation(rememberedPhoneCount_ != 0);
  resetSession();
  state_.paired = rememberedPhoneCount_ != 0;
  state_.pairingCode = 0;
  state_.pairingExpiresMs = 0;
  state_.connectedSinceMs = 0;
  state_.link = PhoneLinkState::Advertising;
  strlcpy(state_.displayName, kDisplayName, sizeof(state_.displayName));
}

void PhoneLinkManager::forgetAllPhones() {
  if (sync_) sync_->cancel();
  rememberedPhoneCount_ = 0;
  associationId_ = 0;
  memset(rememberedPhones_, 0, sizeof(rememberedPhones_));
  ++phoneListRevision_;
  persistAssociations();
  transport_.cancelPairing(true);
  resetSession();
  state_.paired = false;
  state_.pairingCode = 0;
  state_.pairingExpiresMs = 0;
  state_.connectedSinceMs = 0;
  state_.link = PhoneLinkState::Advertising;
  strlcpy(state_.displayName, kDisplayName, sizeof(state_.displayName));
}

void PhoneLinkManager::prepareForUsb() {
  if (sync_) sync_->cancel();
}

void PhoneLinkManager::updateLiveData(const LiveTelemetryInput& data,
                                      uint32_t nowMs) {
  currentRideState_ = data.rideState;
  currentRideId_ = data.rideId;
  if ((data.rideState == 0 || data.rideState == 3) &&
      locationState_.fix.available) {
    locationState_.fix.available = false;
  }
  if (!liveDataInitialized_) {
    liveDataInitialized_ = true;
    previousRideState_ = data.rideState;
    previousSdState_ = data.sdState;
    previousRainLocked_ = data.rainLocked;
    previousBatteryLow_ = data.batteryLow;
  }

  if (ready() && state_.authorized) {
    if (data.rideState != previousRideState_) {
      uint8_t event = 0xFF;
      if (data.rideState == 1 && previousRideState_ == 2) event = 2;  // resumed
      else if (data.rideState == 1) event = 0;                        // started
      else if (data.rideState == 2) event = 1;                        // paused
      else if (data.rideState == 3) event = 3;                        // finished
      if (event != 0xFF) {
        uint8_t payload[13] = {event};
        bikeproto::writeU32(payload + 1, data.rideId);
        bikeproto::writeU64(payload + 5, data.elapsedTimeMs);
        sendMessage(bikeproto::MessageType::RideEvent, 0, payload,
                    sizeof(payload), false);
      }
    }
    if (data.sdState != previousSdState_ && data.sdState == 2) {
      uint8_t payload[5] = {0};  // SD_ERROR
      bikeproto::writeU32(payload + 1, data.sdState);
      sendMessage(bikeproto::MessageType::DeviceEvent, 0, payload,
                  sizeof(payload), false);
    }
    if (data.batteryLow && !previousBatteryLow_) {
      uint8_t payload[5] = {1};  // LOW_BATTERY
      bikeproto::writeU32(payload + 1, data.batteryPercent);
      sendMessage(bikeproto::MessageType::DeviceEvent, 0, payload,
                  sizeof(payload), false);
    }
    if (data.rainLocked != previousRainLocked_) {
      uint8_t payload[5] = {2};  // RAIN_LOCK_CHANGED
      bikeproto::writeU32(payload + 1, data.rainLocked ? 1 : 0);
      sendMessage(bikeproto::MessageType::DeviceEvent, 0, payload,
                  sizeof(payload), false);
    }

    if (!lastTelemetryMs_ || nowMs - lastTelemetryMs_ >= 500) {
      uint8_t payload[44] = {0};
      payload[0] = data.rideState;
      payload[1] = data.motionState;
      payload[2] = data.batteryPercent;
      payload[3] = data.sdState;
      bikeproto::writeFloat(payload + 4, data.speedKmh);
      bikeproto::writeFloat(payload + 8, data.distanceM);
      bikeproto::writeFloat(payload + 12, data.averageSpeedKmh);
      bikeproto::writeFloat(payload + 16, data.maxSpeedKmh);
      bikeproto::writeU64(payload + 20, data.movingTimeMs);
      bikeproto::writeU64(payload + 28, data.elapsedTimeMs);
      bikeproto::writeU32(payload + 36, data.pulseCount);
      bikeproto::writeU32(payload + 40, data.rideId);
      if (sendMessage(bikeproto::MessageType::LiveTelemetry, 0, payload,
                      sizeof(payload), false)) {
        lastTelemetryMs_ = nowMs;
      }
    }
  }

  previousRideState_ = data.rideState;
  previousSdState_ = data.sdState;
  previousRainLocked_ = data.rainLocked;
  previousBatteryLow_ = data.batteryLow;
}

bool PhoneLinkManager::sendMessage(bikeproto::MessageType type, uint8_t flags,
                                   const uint8_t* payload,
                                   uint16_t payloadLength, bool indicate) {
  uint8_t encoded[bikeproto::kMaximumFrameBytes];
  uint16_t encodedLength = 0;
  const uint16_t sequence = nextTxSequence_;
  nextTxSequence_ = sequence == 0xFFFF ? 1 : sequence + 1;
  if (!bikeproto::Codec::encode(type, flags, sequence, payload, payloadLength,
                                encoded, sizeof(encoded), encodedLength)) {
    return false;
  }
  return transport_.send(encoded, encodedLength, indicate);
}

bool PhoneLinkManager::sendResponseMessage(
    bikeproto::MessageType type, uint16_t requestSequence,
    const uint8_t* payload, uint16_t payloadLength, bool indicate) {
  return sendResponse(type, requestSequence, payload, payloadLength, indicate);
}

void PhoneLinkManager::sendProtocolError(
    bikeproto::ErrorCode code, bikeproto::MessageType rejectedType,
    uint16_t rejectedSequence, const char* detail) {
  sendError(code, rejectedType, rejectedSequence, detail);
}

void PhoneLinkManager::loadIdentity() {
  const uint64_t mac = ESP.getEfuseMac();
  deviceId_ = mac ? mac : randomU64();
  Preferences prefs;
  if (!prefs.begin("phone_link", true)) return;
  bool rewriteRegistry = false;
  rememberedPhoneCount_ =
      min<uint8_t>(prefs.getUChar("phone_count", 0),
                   kMaximumRememberedPhones);
  const size_t storedBytes = prefs.getBytesLength("phones_v2");
  if (rememberedPhoneCount_ && storedBytes == sizeof(rememberedPhones_)) {
    prefs.getBytes("phones_v2", rememberedPhones_, sizeof(rememberedPhones_));
  } else {
    rewriteRegistry = rememberedPhoneCount_ != 0;
    rememberedPhoneCount_ = 0;
    memset(rememberedPhones_, 0, sizeof(rememberedPhones_));
    const uint64_t legacyAssociation =
        prefs.getULong64("association", 0);
    if (legacyAssociation) {
      rememberedPhones_[0].associationId = legacyAssociation;
      strlcpy(rememberedPhones_[0].displayName, "Android phone",
              sizeof(rememberedPhones_[0].displayName));
      rememberedPhoneCount_ = 1;
      rewriteRegistry = true;
    }
  }
  associationId_ = 0;
  prefs.end();
  if (rewriteRegistry) persistAssociations();
}

void PhoneLinkManager::persistAssociations() {
  Preferences prefs;
  if (!prefs.begin("phone_link", false)) {
    noteError("Cannot persist remembered phones");
    return;
  }
  prefs.putUChar("phone_count", rememberedPhoneCount_);
  prefs.putBytes("phones_v2", rememberedPhones_, sizeof(rememberedPhones_));
  prefs.putULong64("association",
                   rememberedPhoneCount_ ? rememberedPhones_[0].associationId
                                         : 0);
  prefs.end();
}

void PhoneLinkManager::createAssociation() {
  associationId_ = randomU64();
  state_.paired = true;
  transport_.setKnownAssociation(true);
  Serial.println("[BLE] pending phone association created");
}

int8_t PhoneLinkManager::findAssociation(uint64_t associationId) const {
  if (!associationId) return -1;
  for (uint8_t i = 0; i < rememberedPhoneCount_; ++i) {
    if (rememberedPhones_[i].associationId == associationId) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

void PhoneLinkManager::rememberActiveAssociation(const char* displayName) {
  int8_t index = findAssociation(associationId_);
  if (index < 0) {
    if (!associationId_ || !canRememberAnotherPhone()) return;
    index = static_cast<int8_t>(rememberedPhoneCount_++);
    rememberedPhones_[index].associationId = associationId_;
  }
  strlcpy(rememberedPhones_[index].displayName,
          displayName && displayName[0] ? displayName : "Android phone",
          sizeof(rememberedPhones_[index].displayName));
  ++phoneListRevision_;
  persistAssociations();
  transport_.setKnownAssociation(true);
  state_.paired = true;
  Serial.printf("[BLE] remembered phone %u/%u\n",
                static_cast<unsigned>(index + 1),
                static_cast<unsigned>(kMaximumRememberedPhones));
}

void PhoneLinkManager::resetSession() {
  decoder_.reset();
  state_.authorized = false;
  state_.protocolVersion = 0;
  associationCreatedThisSession_ = false;
  associationId_ = 0;
  nextTxSequence_ = 1;
  if (mediaState_.available) {
    mediaState_ = media::MediaState();
    ++mediaRevision_;
  }
  if (navigationState_.available) {
    navigationState_ = navigation::NavigationState();
    ++navigationRevision_;
  }
  locationState_ = phonegeo::LocationState();
}

void PhoneLinkManager::processIncoming() {
  for (;;) {
    const size_t received =
        transport_.readRx(incomingBytes_, sizeof(incomingBytes_));
    if (!received) break;
    decoder_.feed(incomingBytes_, received);
  }

  for (uint8_t budget = 0; budget < 8; ++budget) {
    const bikeproto::DecodeResult result = decoder_.next(incomingFrame_);
    if (result == bikeproto::DecodeResult::NeedMoreData) break;
    if (result == bikeproto::DecodeResult::FrameReady) {
      handleFrame(incomingFrame_);
      continue;
    }
    if (result == bikeproto::DecodeResult::CrcMismatch) {
      noteError("Protocol CRC mismatch");
      sendError(bikeproto::ErrorCode::CrcMismatch,
                bikeproto::MessageType::Error, 0, "CRC mismatch");
    } else if (result == bikeproto::DecodeResult::UnsupportedVersion) {
      noteError("Unsupported protocol version");
      sendError(bikeproto::ErrorCode::UnsupportedVersion,
                bikeproto::MessageType::Error, 0, "unsupported version");
    } else {
      noteError("Malformed protocol frame");
      sendError(bikeproto::ErrorCode::MalformedFrame,
                bikeproto::MessageType::Error, 0, "malformed frame");
    }
  }
}

void PhoneLinkManager::handleFrame(const bikeproto::Frame& frame) {
  if (frame.type == bikeproto::MessageType::Hello) {
    handleHello(frame);
    return;
  }
  if (!state_.protocolVersion) {
    sendError(bikeproto::ErrorCode::InvalidState, frame.type, frame.sequence,
              "HELLO required");
    return;
  }
  if (frame.type == bikeproto::MessageType::Ping) {
    uint8_t encoded[bikeproto::kMaximumFrameBytes];
    uint16_t encodedLength = 0;
    if (bikeproto::Codec::encode(
            bikeproto::MessageType::Pong, bikeproto::FrameFlags::Response,
            frame.sequence, frame.payload, frame.payloadLength, encoded,
            sizeof(encoded), encodedLength)) {
      transport_.send(encoded, encodedLength, false);
    }
    return;
  }
  if (frame.type == bikeproto::MessageType::TimeSync) {
    handleTimeSync(frame);
    return;
  }
  if (frame.type == bikeproto::MessageType::MediaState) {
    handleMediaState(frame);
    return;
  }
  if (frame.type == bikeproto::MessageType::NavigationState) {
    handleNavigationState(frame);
    return;
  }
  if (frame.type == bikeproto::MessageType::LocationFix) {
    handleLocationFix(frame);
    return;
  }
  if (config_ && config_->handleFrame(frame, *this)) return;
  if (sync_ && sync_->handleFrame(frame, *this)) return;

  if ((frame.flags & bikeproto::FrameFlags::AckRequired) != 0) {
    sendError(bikeproto::ErrorCode::UnsupportedMessage, frame.type,
              frame.sequence, "message not implemented");
  }
}

void PhoneLinkManager::handleMediaState(const bikeproto::Frame& frame) {
  if (!state_.authorized) {
    sendError(bikeproto::ErrorCode::NotAuthorized, frame.type, frame.sequence,
              "authorized HELLO required");
    return;
  }
  if (frame.payloadLength < 24) {
    sendError(bikeproto::ErrorCode::MalformedFrame, frame.type, frame.sequence,
              "short media state");
    return;
  }
  size_t offset = 0;
  media::MediaState next;
  next.available = frame.payload[offset++] != 0;
  next.playing = frame.payload[offset++] != 0;
  next.supportedActions = bikeproto::readU32(frame.payload + offset);
  offset += 4;
  next.durationMs = bikeproto::readU64(frame.payload + offset);
  offset += 8;
  next.positionMs = bikeproto::readU64(frame.payload + offset);
  offset += 8;
  next.receivedAtMs = millis();

  auto readText = [&](char* output, size_t capacity) -> bool {
    if (offset >= frame.payloadLength) return false;
    const uint8_t length = frame.payload[offset++];
    if (length >= capacity || offset + length > frame.payloadLength) return false;
    if (length) memcpy(output, frame.payload + offset, length);
    output[length] = '\0';
    offset += length;
    return true;
  };
  if (!readText(next.player, sizeof(next.player)) ||
      !readText(next.title, sizeof(next.title)) ||
      !readText(next.artist, sizeof(next.artist)) ||
      offset != frame.payloadLength) {
    sendError(bikeproto::ErrorCode::MalformedFrame, frame.type, frame.sequence,
              "invalid media strings");
    return;
  }
  if (!next.available) next = media::MediaState();
  mediaState_ = next;
  ++mediaRevision_;
}

void PhoneLinkManager::handleNavigationState(const bikeproto::Frame& frame) {
  if (!state_.authorized) {
    sendError(bikeproto::ErrorCode::NotAuthorized, frame.type, frame.sequence,
              "authorized HELLO required");
    return;
  }
  if (frame.payloadLength < 25) {
    sendError(bikeproto::ErrorCode::MalformedFrame, frame.type, frame.sequence,
              "short navigation state");
    return;
  }
  navigation::NavigationState next;
  size_t offset = 0;
  next.available = frame.payload[offset++] != 0;
  const uint8_t lifecycle = frame.payload[offset++];
  if (lifecycle > static_cast<uint8_t>(navigation::Lifecycle::Error)) {
    sendError(bikeproto::ErrorCode::InvalidValue, frame.type, frame.sequence,
              "invalid navigation lifecycle");
    return;
  }
  next.lifecycle = static_cast<navigation::Lifecycle>(lifecycle);
  next.maneuver = static_cast<navigation::Maneuver>(frame.payload[offset++]);
  next.nextManeuver =
      static_cast<navigation::Maneuver>(frame.payload[offset++]);
  next.distanceToManeuverM = bikeproto::readU32(frame.payload + offset);
  offset += 4;
  next.nextDistanceM = bikeproto::readU32(frame.payload + offset);
  offset += 4;
  next.remainingDistanceM = bikeproto::readU32(frame.payload + offset);
  offset += 4;
  next.etaUtcMs = static_cast<int64_t>(bikeproto::readU64(frame.payload + offset));
  offset += 8;
  const uint8_t streetLength = frame.payload[offset++];
  if (streetLength >= sizeof(next.street) ||
      offset + streetLength != frame.payloadLength) {
    sendError(bikeproto::ErrorCode::MalformedFrame, frame.type, frame.sequence,
              "invalid street");
    return;
  }
  if (streetLength) {
    memcpy(next.street, frame.payload + offset, streetLength);
    next.street[streetLength] = '\0';
  }
  if (!next.available) next = navigation::NavigationState();
  navigationState_ = next;
  ++navigationRevision_;
}

void PhoneLinkManager::handleLocationFix(const bikeproto::Frame& frame) {
  auto reject = [&](bikeproto::ErrorCode code, const char* detail) {
    ++locationState_.rejectedCount;
    sendError(code, frame.type, frame.sequence, detail);
  };
  if (!state_.authorized ||
      (state_.capabilities & bikeproto::Capability::GpsAssist) == 0 ||
      (frame.flags & bikeproto::FrameFlags::Privileged) == 0) {
    reject(bikeproto::ErrorCode::NotAuthorized,
           "authorized GPS Assist required");
    return;
  }
  if (frame.payloadLength != 33) {
    reject(bikeproto::ErrorCode::MalformedFrame,
           "location payload must be 33 bytes");
    return;
  }

  phonegeo::LocationFix next;
  next.rideId = bikeproto::readU32(frame.payload);
  next.timestampUtcMs = bikeproto::readU64(frame.payload + 4);
  next.flags = frame.payload[12];
  next.latitudeE7 =
      static_cast<int32_t>(bikeproto::readU32(frame.payload + 13));
  next.longitudeE7 =
      static_cast<int32_t>(bikeproto::readU32(frame.payload + 17));
  next.altitudeMm =
      static_cast<int32_t>(bikeproto::readU32(frame.payload + 21));
  next.accuracyMm = bikeproto::readU32(frame.payload + 25);
  next.speedMmps = bikeproto::readU32(frame.payload + 29);
  next.receivedAtMs = millis();
  next.available = true;

  if ((currentRideState_ != 1 && currentRideState_ != 2) ||
      !currentRideId_ || next.rideId != currentRideId_) {
    reject(bikeproto::ErrorCode::InvalidState,
           "location does not match active ride");
    return;
  }
  if (!next.validValues()) {
    reject(bikeproto::ErrorCode::InvalidValue,
           "invalid location values");
    return;
  }
  locationState_.fix = next;
  ++locationState_.acceptedCount;
}

bool PhoneLinkManager::sendMediaAction(media::Action action,
                                       uint64_t positionMs) {
  if (!ready() || !state_.authorized || !mediaState_.available) return false;
  uint8_t payload[9] = {static_cast<uint8_t>(action)};
  bikeproto::writeU64(payload + 1, positionMs);
  return sendMessage(bikeproto::MessageType::MediaAction,
                     bikeproto::FrameFlags::Privileged, payload,
                     sizeof(payload), false);
}

void PhoneLinkManager::handleTimeSync(const bikeproto::Frame& frame) {
  if (!state_.authorized || frame.payloadLength < 13) {
    sendError(bikeproto::ErrorCode::NotAuthorized, frame.type, frame.sequence,
              "authorized HELLO required");
    return;
  }
  const uint64_t rawEpoch =
      static_cast<uint64_t>(bikeproto::readU32(frame.payload)) |
      (static_cast<uint64_t>(bikeproto::readU32(frame.payload + 4)) << 32);
  const int64_t epochMs = static_cast<int64_t>(rawEpoch);
  const int32_t utcOffset =
      static_cast<int32_t>(bikeproto::readU32(frame.payload + 8));
  const uint8_t timezoneLength = frame.payload[12];
  if (timezoneLength >= 48 ||
      13U + timezoneLength != frame.payloadLength) {
    sendError(bikeproto::ErrorCode::InvalidValue, frame.type, frame.sequence,
              "invalid timezone");
    return;
  }
  char timezone[48] = {0};
  if (timezoneLength) {
    memcpy(timezone, frame.payload + 13, timezoneLength);
  }
  if (!clock_.sync(epochMs, utcOffset, timezone,
                   ClockManager::monotonicMs())) {
    sendError(bikeproto::ErrorCode::InvalidValue, frame.type, frame.sequence,
              "invalid clock");
    return;
  }
  state_.clockSynced = true;
  state_.lastTimeSyncMs = millis();
  sendResponse(bikeproto::MessageType::TimeSync, frame.sequence, nullptr, 0,
               true);
  Serial.printf("[PROTO] time synchronized (%s)\n", timezone);
}

bool PhoneLinkManager::sendResponse(bikeproto::MessageType type,
                                    uint16_t requestSequence,
                                    const uint8_t* payload,
                                    uint16_t payloadLength, bool indicate) {
  uint8_t encoded[bikeproto::kMaximumFrameBytes];
  uint16_t encodedLength = 0;
  if (bikeproto::Codec::encode(
          type, bikeproto::FrameFlags::Response, requestSequence, payload,
          payloadLength, encoded, sizeof(encoded), encodedLength)) {
    return transport_.send(encoded, encodedLength, indicate);
  }
  return false;
}

void PhoneLinkManager::handleHello(const bikeproto::Frame& frame) {
  if (!transport_.authenticated()) {
    sendError(bikeproto::ErrorCode::NotAuthorized, frame.type, frame.sequence,
              "encrypted link required");
    return;
  }
  size_t offset = 0;
  if (!helloHasProtocolOne(frame.payload, frame.payloadLength, offset) ||
      offset + 8 + 4 + 1 > frame.payloadLength) {
    sendError(bikeproto::ErrorCode::ProtocolMismatch, frame.type,
              frame.sequence, "no common protocol");
    return;
  }

  const uint64_t suppliedAssociation =
      static_cast<uint64_t>(bikeproto::readU32(frame.payload + offset)) |
      (static_cast<uint64_t>(bikeproto::readU32(frame.payload + offset + 4))
       << 32);
  offset += 8;
  const uint32_t requestedCapabilities =
      bikeproto::readU32(frame.payload + offset);
  offset += 4;
  const uint8_t nameLength = frame.payload[offset++];
  if (offset + nameLength > frame.payloadLength) {
    sendError(bikeproto::ErrorCode::MalformedFrame, frame.type,
              frame.sequence, "invalid HELLO length");
    return;
  }
  const size_t copyLength =
      std::min<size_t>(nameLength, sizeof(state_.displayName) - 1);
  if (copyLength) memcpy(state_.displayName, frame.payload + offset, copyLength);
  state_.displayName[copyLength] = '\0';
  if (!copyLength) {
    strlcpy(state_.displayName, "Android phone", sizeof(state_.displayName));
  }

  state_.protocolVersion = bikeproto::kProtocolVersion;
  state_.capabilities =
      requestedCapabilities & bikeproto::kImplementedCapabilities;
  const int8_t rememberedIndex = findAssociation(suppliedAssociation);
  const bool newAssociation =
      associationCreatedThisSession_ && rememberedIndex < 0;
  if (rememberedIndex >= 0) {
    associationId_ = rememberedPhones_[rememberedIndex].associationId;
    associationCreatedThisSession_ = false;
  } else if (!associationCreatedThisSession_) {
    associationId_ = 0;
  }
  state_.authorized =
      associationId_ != 0 &&
      (associationCreatedThisSession_ ||
       suppliedAssociation == associationId_);
  if (!state_.authorized) {
    state_.link = PhoneLinkState::Authenticating;
    sendError(bikeproto::ErrorCode::NotAuthorized, frame.type,
              frame.sequence, "association mismatch");
    Serial.println("[PROTO] HELLO rejected, association mismatch");
    return;
  }
  rememberActiveAssociation(state_.displayName);
  if (newAssociation) transport_.finishPairing();
  associationCreatedThisSession_ = false;
  state_.link = PhoneLinkState::Ready;
  state_.lastError[0] = '\0';
  sendHelloAck(frame.sequence);
  Serial.printf("[PROTO] HELLO ready, authorized=%s\n",
                state_.authorized ? "yes" : "no");
}

void PhoneLinkManager::sendHelloAck(uint16_t requestSequence) {
  uint8_t payload[64] = {0};
  size_t offset = 0;
  payload[offset++] = bikeproto::kProtocolVersion;
  payload[offset++] = 2;
  payload[offset++] = 2;
  payload[offset++] = 0;
  payload[offset++] = 0;
  bikeproto::writeU64(payload + offset, deviceId_);
  offset += 8;
  bikeproto::writeU64(payload + offset, associationId_);
  offset += 8;
  bikeproto::writeU32(payload + offset, bikeproto::kImplementedCapabilities);
  offset += 4;
  payload[offset++] = 2;
  payload[offset++] = 1;
  payload[offset++] = app::RIDE_LOG_FORMAT_VERSION;
  const uint8_t displayNameLength = strlen(kDisplayName);
  payload[offset++] = displayNameLength;
  memcpy(payload + offset, kDisplayName, displayNameLength);
  offset += displayNameLength;

  uint8_t encoded[bikeproto::kMaximumFrameBytes];
  uint16_t encodedLength = 0;
  if (bikeproto::Codec::encode(
          bikeproto::MessageType::HelloAck,
          bikeproto::FrameFlags::Response, requestSequence, payload, offset,
          encoded, sizeof(encoded), encodedLength)) {
    transport_.send(encoded, encodedLength, true);
  }
}

void PhoneLinkManager::sendError(bikeproto::ErrorCode code,
                                 bikeproto::MessageType rejectedType,
                                 uint16_t rejectedSequence,
                                 const char* detail) {
  if (!transport_.authenticated()) return;
  uint8_t payload[80] = {0};
  bikeproto::writeU16(payload, static_cast<uint16_t>(code));
  payload[2] = static_cast<uint8_t>(rejectedType);
  bikeproto::writeU16(payload + 3, rejectedSequence);
  const size_t detailLength =
      detail ? std::min<size_t>(strlen(detail), sizeof(payload) - 6) : 0;
  payload[5] = detailLength;
  if (detailLength) memcpy(payload + 6, detail, detailLength);
  uint8_t encoded[bikeproto::kMaximumFrameBytes];
  uint16_t encodedLength = 0;
  if (bikeproto::Codec::encode(
          bikeproto::MessageType::Error,
          bikeproto::FrameFlags::Response | bikeproto::FrameFlags::Error,
          rejectedSequence, payload, 6 + detailLength, encoded,
          sizeof(encoded), encodedLength)) {
    transport_.send(encoded, encodedLength, true);
  }
}

void PhoneLinkManager::noteError(const char* detail) {
  strlcpy(state_.lastError, detail ? detail : "unknown",
          sizeof(state_.lastError));
  Serial.print("[PROTO] ");
  Serial.println(state_.lastError);
}

bool PhoneLinkManager::helloHasProtocolOne(const uint8_t* payload,
                                           uint16_t length,
                                           size_t& offset) const {
  offset = 0;
  if (!payload || length < 4) return false;
  offset = 3;
  const uint8_t count = payload[offset++];
  if (!count || offset + count > length) return false;
  bool supported = false;
  for (uint8_t i = 0; i < count; ++i) {
    if (payload[offset + i] == bikeproto::kProtocolVersion) supported = true;
  }
  offset += count;
  return supported;
}
