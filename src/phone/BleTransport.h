#pragma once

#include <Arduino.h>
#include <BLECharacteristic.h>
#include <BLEServer.h>
#include <BLESecurity.h>

class BLE2902;
class BLEService;

class BleTransport final : private BLEServerCallbacks,
                           private BLECharacteristicCallbacks,
                           private BLESecurityCallbacks {
 public:
  static constexpr uint16_t kPreferredMtu = 247;
  static constexpr uint32_t kPairingWindowMs = 120000;

  bool begin(const char* deviceName, const uint8_t* deviceInfo,
             uint16_t deviceInfoLength);
  void update(uint32_t nowMs);
  void startPairing(uint32_t nowMs);
  void finishPairing();
  void cancelPairing(bool clearBonds = false);
  void setKnownAssociation(bool known) { knownAssociation_ = known; }

  bool connected() const { return connected_; }
  bool authenticated() const { return authenticated_; }
  bool hasStoredBond() const;
  bool cancellingPairing() const { return clearBondsPending_; }
  bool pairingActive(uint32_t nowMs) const;
  uint32_t pairingCode() const { return pairingCode_; }
  uint32_t pairingExpiresMs() const { return pairingExpiresMs_; }
  uint16_t negotiatedMtu() const { return negotiatedMtu_; }
  bool rxOverflowed();

  size_t readRx(uint8_t* destination, size_t capacity);
  bool send(const uint8_t* data, uint16_t length, bool indicate = false);

 private:
  void onConnect(BLEServer* server) override;
  void onDisconnect(BLEServer* server) override;
  void onMtuChanged(BLEServer* server, esp_ble_gatts_cb_param_t* param) override;
  void onWrite(BLECharacteristic* characteristic,
               esp_ble_gatts_cb_param_t* param) override;
  uint32_t onPassKeyRequest() override;
  void onPassKeyNotify(uint32_t passKey) override;
  bool onSecurityRequest() override;
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override;
  bool onConfirmPIN(uint32_t pin) override;

  void queueRx(const uint8_t* data, size_t length);
  void flushTx(uint32_t nowMs);
  void clearStoredBonds();
  void configureBondingSecurity();

  static constexpr size_t kRxBufferBytes = 1152;
  static constexpr uint8_t kTxQueueDepth = 16;
  static constexpr uint16_t kMaximumAttPayloadBytes = kPreferredMtu - 3;
  BLEServer* server_ = nullptr;
  BLEService* service_ = nullptr;
  BLECharacteristic* deviceInfo_ = nullptr;
  BLECharacteristic* rx_ = nullptr;
  BLECharacteristic* tx_ = nullptr;
  BLESecurity* security_ = nullptr;
  BLE2902* txDescriptor_ = nullptr;

  portMUX_TYPE rxMux_ = portMUX_INITIALIZER_UNLOCKED;
  uint8_t rxBuffer_[kRxBufferBytes] = {0};
  volatile size_t rxRead_ = 0;
  volatile size_t rxWrite_ = 0;
  volatile size_t rxCount_ = 0;
  volatile bool rxOverflow_ = false;
  uint8_t txFrames_[kTxQueueDepth][kMaximumAttPayloadBytes] = {{0}};
  uint16_t txLengths_[kTxQueueDepth] = {0};
  uint8_t txRead_ = 0;
  uint8_t txWrite_ = 0;
  uint8_t txCount_ = 0;
  uint32_t lastTxMs_ = 0;

  volatile bool connected_ = false;
  volatile bool authenticated_ = false;
  volatile bool restartAdvertising_ = false;
  volatile bool clearBondsPending_ = false;
  volatile bool authResultPending_ = false;
  volatile bool lastAuthSucceeded_ = false;
  volatile uint16_t negotiatedMtu_ = 23;
  uint32_t pairingCode_ = 0;
  uint32_t pairingExpiresMs_ = 0;
  bool knownAssociation_ = false;
};
