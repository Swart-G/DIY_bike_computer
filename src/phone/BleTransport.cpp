#include "phone/BleTransport.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEService.h>
#include <esp_gap_ble_api.h>
#include <esp_system.h>

#include "phone/ProtocolMessages.h"

bool BleTransport::begin(const char* deviceName, const uint8_t* deviceInfo,
                         uint16_t deviceInfoLength) {
  BLEDevice::init(deviceName ? deviceName : "DIY Bike Computer");
  BLEDevice::setMTU(kPreferredMtu);
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(this);

  security_ = new BLESecurity();
  if (!security_) return false;
  security_->setStaticPIN(100000UL + (esp_random() % 900000UL));
  configureBondingSecurity();

  server_ = BLEDevice::createServer();
  if (!server_) return false;
  server_->setCallbacks(this);
  service_ =
      server_->createService(BLEUUID(std::string(bikeproto::kServiceUuid)), 12);
  if (!service_) return false;

  deviceInfo_ = service_->createCharacteristic(
      bikeproto::kDeviceInfoUuid, BLECharacteristic::PROPERTY_READ);
  rx_ = service_->createCharacteristic(
      bikeproto::kRxUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  tx_ = service_->createCharacteristic(
      bikeproto::kTxUuid,
      BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);
  if (!deviceInfo_ || !rx_ || !tx_) return false;

  deviceInfo_->setValue(const_cast<uint8_t*>(deviceInfo),
                        deviceInfo ? deviceInfoLength : 0);
  deviceInfo_->setAccessPermissions(ESP_GATT_PERM_READ);
  rx_->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED |
                            ESP_GATT_PERM_ENCRYPT_KEY_SIZE(16));
  tx_->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED |
                            ESP_GATT_PERM_ENCRYPT_KEY_SIZE(16));
  rx_->setCallbacks(this);

  txDescriptor_ = new BLE2902();
  if (!txDescriptor_) return false;
  txDescriptor_->setNotifications(true);
  txDescriptor_->setIndications(true);
  txDescriptor_->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED |
                                      ESP_GATT_PERM_WRITE_ENCRYPTED |
                                      ESP_GATT_PERM_ENCRYPT_KEY_SIZE(16));
  tx_->addDescriptor(txDescriptor_);

  service_->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  if (!advertising) return false;
  advertising->addServiceUUID(bikeproto::kServiceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  advertising->start();
  Serial.println("[BLE] advertising");
  return true;
}

void BleTransport::update(uint32_t nowMs) {
  if (clearBondsPending_ && !connected_) clearStoredBonds();
  if (restartAdvertising_) {
    restartAdvertising_ = false;
    BLEDevice::startAdvertising();
    Serial.println("[BLE] advertising resumed");
  }
  if (pairingCode_ && !pairingActive(nowMs)) {
    pairingCode_ = 0;
    pairingExpiresMs_ = 0;
    Serial.println("[BLE] pairing window expired");
  }
  if (authResultPending_) {
    authResultPending_ = false;
    Serial.println(lastAuthSucceeded_ ? "[BLE] authentication complete"
                                      : "[BLE] authentication failed");
  }
  flushTx(nowMs);
}

void BleTransport::startPairing(uint32_t nowMs) {
  pairingCode_ = 100000UL + (esp_random() % 900000UL);
  pairingExpiresMs_ = nowMs + kPairingWindowMs;
  if (security_) {
    security_->setStaticPIN(pairingCode_);
    // ESP32 Arduino's setStaticPIN() changes AUTHEN_REQ to SC_ONLY. Restore
    // bonding every time the runtime passkey changes so reconnects reuse LTK.
    configureBondingSecurity();
  }
  Serial.printf("[BLE] pairing enabled for %lu ms\n",
                static_cast<unsigned long>(kPairingWindowMs));
}

void BleTransport::cancelPairing() {
  pairingCode_ = 0;
  pairingExpiresMs_ = 0;
  knownAssociation_ = false;
  restartAdvertising_ = false;
  clearBondsPending_ = true;
  portENTER_CRITICAL(&rxMux_);
  rxRead_ = rxWrite_ = rxCount_ = 0;
  portEXIT_CRITICAL(&rxMux_);
  txRead_ = txWrite_ = txCount_ = 0;
  if (server_ && server_->getConnectedCount()) {
    server_->disconnect(server_->getConnId());
  } else {
    clearStoredBonds();
    BLEDevice::startAdvertising();
  }
  Serial.println("[BLE] pairing cancelled by user");
}

bool BleTransport::pairingActive(uint32_t nowMs) const {
  return pairingCode_ != 0 &&
         static_cast<int32_t>(pairingExpiresMs_ - nowMs) > 0;
}

bool BleTransport::hasStoredBond() const {
  return esp_ble_get_bond_device_num() > 0;
}

bool BleTransport::rxOverflowed() {
  portENTER_CRITICAL(&rxMux_);
  const bool result = rxOverflow_;
  rxOverflow_ = false;
  portEXIT_CRITICAL(&rxMux_);
  return result;
}

size_t BleTransport::readRx(uint8_t* destination, size_t capacity) {
  if (!destination || !capacity) return 0;
  portENTER_CRITICAL(&rxMux_);
  const size_t count = capacity < rxCount_ ? capacity : rxCount_;
  for (size_t i = 0; i < count; ++i) {
    destination[i] = rxBuffer_[rxRead_];
    rxRead_ = (rxRead_ + 1) % kRxBufferBytes;
  }
  rxCount_ -= count;
  portEXIT_CRITICAL(&rxMux_);
  return count;
}

bool BleTransport::send(const uint8_t* data, uint16_t length, bool indicate) {
  if (!connected_ || !authenticated_ || !tx_ || !data || !length) {
    return false;
  }
  const uint16_t negotiatedPayload =
      negotiatedMtu_ > 3 ? negotiatedMtu_ - 3 : 20;
  const uint16_t attPayload =
      min<uint16_t>(negotiatedPayload, kMaximumAttPayloadBytes);
  const uint8_t packets =
      static_cast<uint8_t>((length + attPayload - 1) / attPayload);
  (void)indicate;
  if (packets > kTxQueueDepth - txCount_) return false;
  uint16_t offset = 0;
  while (offset < length) {
    const uint16_t packetLength =
        min<uint16_t>(attPayload, length - offset);
    memcpy(txFrames_[txWrite_], data + offset, packetLength);
    txLengths_[txWrite_] = packetLength;
    txWrite_ = (txWrite_ + 1) % kTxQueueDepth;
    ++txCount_;
    offset += packetLength;
  }
  return true;
}

void BleTransport::onConnect(BLEServer*) {
  connected_ = true;
  authenticated_ = false;
  negotiatedMtu_ = 23;
}

void BleTransport::onDisconnect(BLEServer*) {
  connected_ = false;
  authenticated_ = false;
  negotiatedMtu_ = 23;
  restartAdvertising_ = true;
  portENTER_CRITICAL(&rxMux_);
  rxRead_ = rxWrite_ = rxCount_ = 0;
  portEXIT_CRITICAL(&rxMux_);
  txRead_ = txWrite_ = txCount_ = 0;
}

void BleTransport::onMtuChanged(BLEServer*, esp_ble_gatts_cb_param_t* param) {
  if (param) negotiatedMtu_ = param->mtu.mtu;
}

void BleTransport::onWrite(BLECharacteristic* characteristic,
                           esp_ble_gatts_cb_param_t*) {
  if (characteristic != rx_) return;
  const std::string value = characteristic->getValue();
  queueRx(reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

uint32_t BleTransport::onPassKeyRequest() { return pairingCode_; }

void BleTransport::onPassKeyNotify(uint32_t passKey) {
  if (passKey >= 100000 && passKey <= 999999) pairingCode_ = passKey;
}

bool BleTransport::onSecurityRequest() {
  return pairingCode_ != 0 || knownAssociation_;
}

void BleTransport::onAuthenticationComplete(esp_ble_auth_cmpl_t result) {
  authenticated_ = result.success;
  lastAuthSucceeded_ = result.success;
  authResultPending_ = true;
}

bool BleTransport::onConfirmPIN(uint32_t pin) {
  return pairingCode_ != 0 && pin == pairingCode_;
}

void BleTransport::queueRx(const uint8_t* data, size_t length) {
  if (!data || !length) return;
  portENTER_CRITICAL(&rxMux_);
  if (length > kRxBufferBytes - rxCount_) {
    rxOverflow_ = true;
    portEXIT_CRITICAL(&rxMux_);
    return;
  }
  for (size_t i = 0; i < length; ++i) {
    rxBuffer_[rxWrite_] = data[i];
    rxWrite_ = (rxWrite_ + 1) % kRxBufferBytes;
  }
  rxCount_ += length;
  portEXIT_CRITICAL(&rxMux_);
}

void BleTransport::flushTx(uint32_t nowMs) {
  if (!connected_ || !authenticated_ || !tx_ || !txCount_ ||
      nowMs - lastTxMs_ < 8) {
    return;
  }
  const uint16_t length = txLengths_[txRead_];
  const uint16_t attPayload =
      min<uint16_t>(negotiatedMtu_ > 3 ? negotiatedMtu_ - 3 : 20,
                    kMaximumAttPayloadBytes);
  if (length > attPayload) {
    txRead_ = (txRead_ + 1) % kTxQueueDepth;
    --txCount_;
    return;
  }
  tx_->setValue(txFrames_[txRead_], length);
  // ESP32 BLE Arduino's indicate() blocks for up to one second. Protocol ACKs
  // provide reliability, so queued GATT sends always use nonblocking notify.
  tx_->notify();
  txRead_ = (txRead_ + 1) % kTxQueueDepth;
  --txCount_;
  lastTxMs_ = nowMs;
}

void BleTransport::clearStoredBonds() {
  clearBondsPending_ = false;
  constexpr int kMaximumBonds = 8;
  esp_ble_bond_dev_t bonds[kMaximumBonds] = {};
  int count = min(esp_ble_get_bond_device_num(), kMaximumBonds);
  if (count <= 0 ||
      esp_ble_get_bond_device_list(&count, bonds) != ESP_OK) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    esp_ble_remove_bond_device(bonds[i].bd_addr);
  }
  Serial.printf("[BLE] removed %d stored bond(s)\n", count);
}

void BleTransport::configureBondingSecurity() {
  if (!security_) return;
  security_->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  security_->setCapability(ESP_IO_CAP_OUT);
  security_->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK |
                                  ESP_BLE_ID_KEY_MASK);
  security_->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK |
                                  ESP_BLE_ID_KEY_MASK);
  security_->setKeySize(16);
}
