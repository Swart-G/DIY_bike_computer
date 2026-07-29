#pragma once

#include <Arduino.h>

enum class PhoneLinkState : uint8_t {
  Disabled,
  Advertising,
  Connected,
  Authenticating,
  Ready,
  Transferring,
  Error,
};

struct PhoneState {
  PhoneLinkState link = PhoneLinkState::Disabled;
  bool paired = false;
  bool authorized = false;
  uint16_t negotiatedMtu = 23;
  uint8_t protocolVersion = 0;
  uint32_t capabilities = 0;
  uint32_t pairingCode = 0;
  uint32_t pairingExpiresMs = 0;
  uint32_t connectedSinceMs = 0;
  uint32_t lastSyncMs = 0;
  uint32_t lastTimeSyncMs = 0;
  bool clockSynced = false;
  char displayName[32] = {0};
  char lastError[64] = {0};
};
