#include "sync/RideSyncManager.h"

#include "bus/SharedSpiBus.h"
#include "phone/PhoneLinkManager.h"
#include "storage/StorageManager.h"

namespace {
class SdGuard {
 public:
  SdGuard() : guard_(true) {}

 private:
  hw::SharedSpiBusGuard guard_;
};
}  // namespace

void RideSyncManager::begin(StorageManager& storage,
                            RideRepository& repository) {
  storage_ = &storage;
  repository_ = &repository;
  cancel();
}

void RideSyncManager::setRuntimeState(uint8_t rideState, bool usbActive) {
  rideState_ = rideState;
  usbActive_ = usbActive;
}

bool RideSyncManager::handleFrame(const bikeproto::Frame& frame,
                                  PhoneLinkManager& phone) {
  switch (frame.type) {
    case bikeproto::MessageType::RideListRequest:
      if (frame.payloadLength != 4) {
        protocolError(phone, bikeproto::ErrorCode::InvalidValue, frame,
                      "invalid list request");
        return true;
      }
      beginListing(phone, frame.sequence);
      return true;
    case bikeproto::MessageType::RideDownloadRequest:
      beginDownload(frame, phone);
      return true;
    case bikeproto::MessageType::FileAck:
      handleAck(frame, phone);
      return true;
    case bikeproto::MessageType::TransferCancel:
      cancel();
      return true;
    default:
      return false;
  }
}

void RideSyncManager::update(PhoneLinkManager& phone, uint32_t nowMs) {
  if (state_ == RideSyncState::Idle || state_ == RideSyncState::Error) return;
  if (!available(phone)) {
    cancel();
    return;
  }
  switch (state_) {
    case RideSyncState::BuildingManifests:
      buildManifestStep(phone);
      break;
    case RideSyncState::SendingChunk:
      sendChunk(phone, true);
      break;
    case RideSyncState::SendingFileBegin:
      sendFileBegin(phone);
      break;
    case RideSyncState::WaitingAck:
      if (static_cast<int32_t>(nowMs - ackDeadlineMs_) >= 0) {
        Serial.println("[SYNC] ACK timeout");
        cancel();
      }
      break;
    default:
      break;
  }
}

void RideSyncManager::cancel() {
  if (crcFile_) crcFile_.close();
  if (transferFile_) transferFile_.close();
  transferRide_ = nullptr;
  transferManifest_ = nullptr;
  transferOffset_ = 0;
  transferChunkLength_ = 0;
  ackDeadlineMs_ = 0;
  state_ = RideSyncState::Idle;
}

bool RideSyncManager::available(const PhoneLinkManager& phone) const {
  return storage_ && repository_ && phone.ready() && phone.state().authorized &&
         storage_->sdAvailable() && !usbActive_ &&
         (rideState_ == 0 || rideState_ == 3);
}

bool RideSyncManager::beginListing(PhoneLinkManager& phone,
                                   uint16_t requestSequence) {
  bikeproto::Frame rejected;
  rejected.type = bikeproto::MessageType::RideListRequest;
  rejected.sequence = requestSequence;
  if (!phone.state().authorized) {
    protocolError(phone, bikeproto::ErrorCode::NotAuthorized, rejected,
                  "association required");
    return false;
  }
  if (usbActive_) {
    protocolError(phone, bikeproto::ErrorCode::StorageBusyUsb, rejected,
                  "USB owns storage");
    return false;
  }
  if (rideState_ == 1 || rideState_ == 2) {
    protocolError(phone, bikeproto::ErrorCode::RideActive, rejected,
                  "active ride");
    return false;
  }
  if (!storage_ || !storage_->sdAvailable()) {
    protocolError(phone, bikeproto::ErrorCode::StorageUnavailable, rejected,
                  "SD unavailable");
    return false;
  }

  cancel();
  String error;
  const uint8_t count =
      repository_->list(*storage_, listScratch_, kMaximumRides, error);
  manifestCount_ = 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (!listScratch_[i].complete || !listScratch_[i].id ||
        strncmp(listScratch_[i].folder, "/rides/", 7) != 0 ||
        strstr(listScratch_[i].folder, "..")) {
      continue;
    }
    manifests_[manifestCount_] = RideManifest();
    manifests_[manifestCount_++].ride = listScratch_[i];
  }
  listRequestSequence_ = requestSequence;
  buildRideIndex_ = 0;
  buildFileId_ = 1;
  listedCount_ = 0;
  globalRevision_ = 2166136261UL;
  state_ = RideSyncState::BuildingManifests;
  Serial.printf("[SYNC] building %u manifests\n", manifestCount_);
  return true;
}

void RideSyncManager::buildManifestStep(PhoneLinkManager& phone) {
  if (buildRideIndex_ >= manifestCount_) {
    if (sendListEnd(phone)) state_ = RideSyncState::WaitingRequest;
    return;
  }
  if (!crcFile_) {
    for (uint8_t attempt = 0; attempt < 4 && buildFileId_ <= 4;
         ++attempt) {
      if (openNextManifestFile()) break;
    }
    if (!crcFile_ && buildFileId_ <= 4) return;
  }
  if (!crcFile_ && buildFileId_ > 4) {
    RideManifest& manifest = manifests_[buildRideIndex_];
    manifest.revision = 2166136261UL;
    for (uint8_t i = 0; i < manifest.fileCount; ++i) {
      const FileManifest& file = manifest.files[i];
      manifest.revision =
          (manifest.revision ^ file.id) * 16777619UL;
      manifest.revision =
          (manifest.revision ^ file.size) * 16777619UL;
      manifest.revision =
          (manifest.revision ^ file.crc32) * 16777619UL;
    }
    globalRevision_ =
        (globalRevision_ ^ manifest.revision) * 16777619UL;
    if (sendCurrentManifest(phone)) {
      ++buildRideIndex_;
      buildFileId_ = 1;
    }
    return;
  }

  uint8_t buffer[kCrcReadBytes];
  size_t bytesRead = 0;
  {
    SdGuard guard;
    bytesRead = crcFile_.read(buffer, sizeof(buffer));
  }
  if (bytesRead) {
    crcAccumulator_ = crc32Update(crcAccumulator_, buffer, bytesRead);
    crcSize_ += bytesRead;
    return;
  }
  finishManifestFile();
  ++buildFileId_;
  if (buildFileId_ <= 4) return;

  buildManifestStep(phone);
}

bool RideSyncManager::openNextManifestFile() {
  if (buildRideIndex_ >= manifestCount_ || buildFileId_ > 4) return false;
  char path[72];
  if (!buildFilePath(manifests_[buildRideIndex_], buildFileId_, path,
                     sizeof(path))) {
    ++buildFileId_;
    return false;
  }
  {
    SdGuard guard;
    crcFile_ = SD.open(path, FILE_READ);
  }
  if (!crcFile_) {
    ++buildFileId_;
    return false;
  }
  crcAccumulator_ = 0xFFFFFFFFUL;
  crcSize_ = 0;
  return true;
}

void RideSyncManager::finishManifestFile() {
  if (crcFile_) crcFile_.close();
  RideManifest& ride = manifests_[buildRideIndex_];
  if (ride.fileCount >= 4) return;
  FileManifest& file = ride.files[ride.fileCount++];
  file.id = buildFileId_;
  file.size = crcSize_;
  file.crc32 = crcAccumulator_ ^ 0xFFFFFFFFUL;
  ride.totalSize += file.size;
}

bool RideSyncManager::sendCurrentManifest(PhoneLinkManager& phone) {
  const RideManifest& manifest = manifests_[buildRideIndex_];
  uint8_t payload[80] = {0};
  size_t offset = 0;
  bikeproto::writeU32(payload + offset, manifest.ride.id);
  offset += 4;
  payload[offset++] = manifest.ride.formatVersion;
  payload[offset++] = 1;
  bikeproto::writeU64(
      payload + offset,
      static_cast<uint64_t>(manifest.ride.startedAtUtcMs));
  offset += 8;
  bikeproto::writeU64(
      payload + offset,
      static_cast<uint64_t>(manifest.ride.finishedAtUtcMs));
  offset += 8;
  bikeproto::writeFloat(payload + offset, manifest.ride.distanceM);
  offset += 4;
  bikeproto::writeU64(payload + offset, manifest.ride.elapsedMs);
  offset += 8;
  bikeproto::writeU32(payload + offset, manifest.revision);
  offset += 4;
  bikeproto::writeU32(payload + offset, manifest.totalSize);
  offset += 4;
  payload[offset++] = manifest.fileCount;
  for (uint8_t i = 0; i < manifest.fileCount; ++i) {
    const FileManifest& file = manifest.files[i];
    payload[offset++] = file.id;
    bikeproto::writeU32(payload + offset, file.size);
    offset += 4;
    bikeproto::writeU32(payload + offset, file.crc32);
    offset += 4;
  }
  const bool sent = phone.sendMessage(bikeproto::MessageType::RideManifest,
                                      bikeproto::FrameFlags::More, payload,
                                      offset, false);
  if (sent) {
    ++listedCount_;
  }
  return sent;
}

bool RideSyncManager::sendListEnd(PhoneLinkManager& phone) {
  uint8_t payload[6] = {0};
  bikeproto::writeU16(payload, listedCount_);
  bikeproto::writeU32(payload + 2, globalRevision_);
  const bool sent = phone.sendResponseMessage(
      bikeproto::MessageType::RideListEnd, listRequestSequence_, payload,
      sizeof(payload), false);
  if (sent) Serial.printf("[SYNC] listed %u rides\n", listedCount_);
  return sent;
}

bool RideSyncManager::beginDownload(const bikeproto::Frame& frame,
                                    PhoneLinkManager& phone) {
  if (!available(phone)) {
    protocolError(phone,
                  usbActive_ ? bikeproto::ErrorCode::StorageBusyUsb
                             : (rideState_ == 1 || rideState_ == 2
                                    ? bikeproto::ErrorCode::RideActive
                                    : bikeproto::ErrorCode::StorageUnavailable),
                  frame, "transfer unavailable");
    return false;
  }
  if (frame.payloadLength != 13) {
    protocolError(phone, bikeproto::ErrorCode::InvalidValue, frame,
                  "invalid download request");
    return false;
  }
  const uint32_t rideId = bikeproto::readU32(frame.payload);
  const uint8_t fileId = frame.payload[4];
  const uint32_t resumeOffset = bikeproto::readU32(frame.payload + 5);
  const uint32_t expectedCrc = bikeproto::readU32(frame.payload + 9);
  RideManifest* ride = findRide(rideId);
  FileManifest* file = ride ? findFile(*ride, fileId) : nullptr;
  if (!ride || !file) {
    protocolError(phone, bikeproto::ErrorCode::NotFound, frame,
                  "ride/file not found");
    return false;
  }
  if (resumeOffset > file->size ||
      (expectedCrc && expectedCrc != file->crc32)) {
    protocolError(phone, bikeproto::ErrorCode::IntegrityFailed, frame,
                  "stale resume metadata");
    return false;
  }
  const uint16_t mtu = phone.negotiatedMtu();
  if (mtu <= 27) {
    protocolError(phone, bikeproto::ErrorCode::Busy, frame,
                  "ATT MTU too small");
    return false;
  }

  cancel();
  char path[72];
  if (!buildFilePath(*ride, fileId, path, sizeof(path))) {
    protocolError(phone, bikeproto::ErrorCode::NotFound, frame,
                  "invalid file ID");
    return false;
  }
  {
    SdGuard guard;
    transferFile_ = SD.open(path, FILE_READ);
    if (transferFile_) transferFile_.seek(resumeOffset);
  }
  if (!transferFile_) {
    protocolError(phone, bikeproto::ErrorCode::StorageUnavailable, frame,
                  "cannot open file");
    return false;
  }
  transferRide_ = ride;
  transferManifest_ = file;
  transferOffset_ = resumeOffset;
  transferChunkSequence_ = 0;
  const uint16_t mtuChunk = mtu - 27;
  transferChunkMaximum_ =
      mtuChunk < kMaximumChunkBytes ? mtuChunk : kMaximumChunkBytes;

  transferRequestSequence_ = frame.sequence;
  state_ = RideSyncState::SendingFileBegin;
  sendFileBegin(phone);
  Serial.printf("[SYNC] file %u ride %lu from %lu\n", fileId,
                static_cast<unsigned long>(rideId),
                static_cast<unsigned long>(resumeOffset));
  return true;
}

void RideSyncManager::sendFileBegin(PhoneLinkManager& phone) {
  if (!transferRide_ || !transferManifest_) {
    cancel();
    return;
  }
  uint8_t payload[19] = {0};
  bikeproto::writeU32(payload, transferRide_->ride.id);
  payload[4] = transferManifest_->id;
  bikeproto::writeU32(payload + 5, transferManifest_->size);
  bikeproto::writeU32(payload + 9, transferOffset_);
  bikeproto::writeU32(payload + 13, transferManifest_->crc32);
  bikeproto::writeU16(payload + 17, transferChunkMaximum_);
  if (phone.sendResponseMessage(bikeproto::MessageType::FileBegin,
                                transferRequestSequence_, payload,
                                sizeof(payload), false)) {
    state_ = RideSyncState::SendingChunk;
  }
}

bool RideSyncManager::handleAck(const bikeproto::Frame& frame,
                                PhoneLinkManager& phone) {
  if (state_ != RideSyncState::WaitingAck || frame.payloadLength != 12 ||
      !transferRide_ || !transferManifest_) {
    protocolError(phone, bikeproto::ErrorCode::InvalidState, frame,
                  "no chunk awaiting ACK");
    return false;
  }
  const uint32_t rideId = bikeproto::readU32(frame.payload);
  const uint8_t fileId = frame.payload[4];
  const uint32_t nextOffset = bikeproto::readU32(frame.payload + 5);
  const uint16_t sequence = bikeproto::readU16(frame.payload + 9);
  const uint8_t status = frame.payload[11];
  if (rideId != transferRide_->ride.id ||
      fileId != transferManifest_->id ||
      sequence != transferChunkSequence_) {
    protocolError(phone, bikeproto::ErrorCode::InvalidValue, frame,
                  "ACK identity mismatch");
    cancel();
    return false;
  }
  if (status == 2) {
    cancel();
    return true;
  }
  if (status == 1) {
    sendChunk(phone, false);
    return true;
  }
  if (status != 0 ||
      nextOffset != transferOffset_ + transferChunkLength_) {
    protocolError(phone, bikeproto::ErrorCode::InvalidValue, frame,
                  "ACK offset mismatch");
    cancel();
    return false;
  }
  transferOffset_ = nextOffset;
  ++transferChunkSequence_;
  state_ = RideSyncState::SendingChunk;
  return true;
}

void RideSyncManager::sendChunk(PhoneLinkManager& phone, bool reread) {
  if (!transferRide_ || !transferManifest_ || !transferFile_) {
    cancel();
    return;
  }
  if (transferOffset_ >= transferManifest_->size) {
    sendFileEnd(phone);
    return;
  }
  if (reread) {
    const uint32_t remaining = transferManifest_->size - transferOffset_;
    transferChunkLength_ =
        remaining < transferChunkMaximum_ ? remaining : transferChunkMaximum_;
    size_t bytesRead = 0;
    {
      SdGuard guard;
      bytesRead = transferFile_.read(transferChunk_, transferChunkLength_);
    }
    if (bytesRead != transferChunkLength_) {
      Serial.println("[SYNC] SD read failure");
      cancel();
      return;
    }
  }
  uint8_t payload[13 + kMaximumChunkBytes] = {0};
  bikeproto::writeU32(payload, transferRide_->ride.id);
  payload[4] = transferManifest_->id;
  bikeproto::writeU32(payload + 5, transferOffset_);
  bikeproto::writeU16(payload + 9, transferChunkSequence_);
  bikeproto::writeU16(payload + 11, transferChunkLength_);
  memcpy(payload + 13, transferChunk_, transferChunkLength_);
  if (phone.sendMessage(bikeproto::MessageType::FileChunk,
                        bikeproto::FrameFlags::AckRequired, payload,
                        13 + transferChunkLength_, false)) {
    state_ = RideSyncState::WaitingAck;
    ackDeadlineMs_ = millis() + kAckTimeoutMs;
  }
}

void RideSyncManager::sendFileEnd(PhoneLinkManager& phone) {
  uint8_t payload[13] = {0};
  bikeproto::writeU32(payload, transferRide_->ride.id);
  payload[4] = transferManifest_->id;
  bikeproto::writeU32(payload + 5, transferManifest_->size);
  bikeproto::writeU32(payload + 9, transferManifest_->crc32);
  if (!phone.sendMessage(bikeproto::MessageType::FileEnd, 0, payload,
                         sizeof(payload), false)) {
    return;
  }
  transferFile_.close();
  lastCompletedMs_ = millis();
  transferRide_ = nullptr;
  transferManifest_ = nullptr;
  state_ = RideSyncState::WaitingRequest;
}

RideSyncManager::RideManifest* RideSyncManager::findRide(uint32_t rideId) {
  for (uint8_t i = 0; i < manifestCount_; ++i) {
    if (manifests_[i].ride.id == rideId) return &manifests_[i];
  }
  return nullptr;
}

RideSyncManager::FileManifest* RideSyncManager::findFile(
    RideManifest& ride, uint8_t fileId) {
  for (uint8_t i = 0; i < ride.fileCount; ++i) {
    if (ride.files[i].id == fileId) return &ride.files[i];
  }
  return nullptr;
}

bool RideSyncManager::buildFilePath(const RideManifest& ride, uint8_t fileId,
                                    char* path, size_t capacity) const {
  const char* name = fileName(fileId);
  if (!name || !path || strncmp(ride.ride.folder, "/rides/", 7) != 0 ||
      strstr(ride.ride.folder, "..")) {
    return false;
  }
  const int length =
      snprintf(path, capacity, "%s/%s", ride.ride.folder, name);
  return length > 0 && static_cast<size_t>(length) < capacity;
}

void RideSyncManager::protocolError(PhoneLinkManager& phone,
                                    bikeproto::ErrorCode code,
                                    const bikeproto::Frame& frame,
                                    const char* detail) {
  phone.sendProtocolError(code, frame.type, frame.sequence, detail);
}

uint32_t RideSyncManager::crc32Update(uint32_t crc, const uint8_t* data,
                                      size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
    }
  }
  return crc;
}

const char* RideSyncManager::fileName(uint8_t fileId) {
  switch (fileId) {
    case 1: return "meta.json";
    case 2: return "samples.csv";
    case 3: return "events.csv";
    case 4: return "summary.json";
    default: return nullptr;
  }
}
