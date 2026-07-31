#include "dev/DevMonitor.h"

#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <HWCDC.h>

#include "battery/BatteryMonitor.h"
#include "config/ConfigSyncManager.h"
#include "display/DisplayManager.h"
#include "led/SpeedTrendLed.h"
#include "phone/PhoneLinkManager.h"
#include "speed/HallSensor.h"
#include "speed/RideStateMachine.h"
#include "speed/SpeedCalculator.h"
#include "storage/RideLogger.h"
#include "storage/StorageManager.h"
#include "sync/RideSyncManager.h"
#include "touch/TouchManager.h"
#include "ui/UiApp.h"
#include "ui_exact/screen_assets.h"
#include "usb/UsbMassStorageManager.h"

namespace {

// Native ESP32-S3 USB Serial/JTAG. The production board deliberately keeps
// Arduino Serial on UART0, so Dev Mode owns this endpoint explicitly.
HWCDC g_devUsb;

}  // namespace

void DevMonitor::attach(DisplayManager& display, TouchManager& touch,
                       StorageManager& storage, UsbMassStorageManager& usb,
                       HallSensor& sensor, SpeedCalculator& speed,
                       SpeedTrendLed& speedTrend, RideStateMachine& ride,
                       BatteryMonitor& battery, RideLogger& logger,
                       PhoneLinkManager& phone, UiApp& ui,
                       RideSyncManager& rideSync,
                       ConfigSyncManager& configSync) {
  display_ = &display;
  touch_ = &touch;
  storage_ = &storage;
  usb_ = &usb;
  sensor_ = &sensor;
  speed_ = &speed;
  speedTrend_ = &speedTrend;
  ride_ = &ride;
  battery_ = &battery;
  logger_ = &logger;
  phone_ = &phone;
  ui_ = &ui;
  rideSync_ = &rideSync;
  configSync_ = &configSync;
}

bool DevMonitor::start() {
  if (active_) return true;
  if (!display_ || !touch_ || !storage_ || !usb_ || !sensor_ || !speed_ ||
      !speedTrend_ || !ride_ || !battery_ || !logger_ || !phone_ || !ui_ ||
      !rideSync_ || !configSync_ || usb_->active()) {
    return false;
  }

  active_ = true;
  bootPending_ = true;
  streamEnabled_ = true;
  reportIntervalMs_ = kDefaultReportIntervalMs;
  inputLength_ = 0;
  inputOverflow_ = false;
  usbStoragePending_ = false;
  usbStorageRequestedMs_ = 0;
  sequence_ = 0;
  droppedDocuments_ = 0;
  lastLoopStartedUs_ = 0;
  lastLoopPeriodUs_ = 0;
  maximumLoopPeriodUs_ = 0;
  maximumLoopPeriodEverUs_ = 0;
  intervalLoopCount_ = 0;
  intervalStartedMs_ = millis();
  lastReportMs_ = intervalStartedMs_;
  g_devUsb.setRxBufferSize(1024);
  g_devUsb.setTxBufferSize(8192);
  g_devUsb.setTxTimeoutMs(5);
  g_devUsb.begin(115200);
  return true;
}

void DevMonitor::stop() {
  if (!active_) return;
  if (speedTrend_) speedTrend_->clearDiagnosticRgb();
  if (ui_) ui_->clearDevPreview();
  active_ = false;
  bootPending_ = false;
  inputLength_ = 0;
  inputOverflow_ = false;
  usbStoragePending_ = false;
  usbStorageRequestedMs_ = 0;
  g_devUsb.end();
}

bool DevMonitor::hostConnected() const {
  return active_ && HWCDC::isConnected();
}

void DevMonitor::noteLoopStart(uint32_t nowUs) {
  if (!active_) return;
  if (lastLoopStartedUs_ != 0) {
    lastLoopPeriodUs_ = nowUs - lastLoopStartedUs_;
    maximumLoopPeriodUs_ = max(maximumLoopPeriodUs_, lastLoopPeriodUs_);
    maximumLoopPeriodEverUs_ =
        max(maximumLoopPeriodEverUs_, lastLoopPeriodUs_);
  }
  lastLoopStartedUs_ = nowUs;
  ++intervalLoopCount_;
}

void DevMonitor::update(uint32_t nowMs) {
  if (!active_) return;
  processInput(nowMs);
  if (usbStoragePending_ &&
      nowMs - usbStorageRequestedMs_ >= 750) {
    // The success response has already been queued. Release Serial/JTAG before
    // TinyUSB takes ownership of the shared internal PHY.
    usbStoragePending_ = false;
    stop();
    ui_->startUsbStorageFromDev();
    return;
  }
  if (bootPending_) {
    emitBoot();
    bootPending_ = false;
    lastReportMs_ = nowMs;
    intervalStartedMs_ = nowMs;
    intervalLoopCount_ = 0;
    maximumLoopPeriodUs_ = 0;
    return;
  }
  if (!streamEnabled_ || nowMs - lastReportMs_ < reportIntervalMs_) return;
  emitSample(nowMs);
  lastReportMs_ = nowMs;
  intervalStartedMs_ = nowMs;
  intervalLoopCount_ = 0;
  maximumLoopPeriodUs_ = 0;
}

void DevMonitor::emitBoot() {
  document_.clear();
  document_["type"] = "boot";
  document_["schema"] = 1;
  document_["firmware"] = app::FIRMWARE_VERSION;
  document_["board"] = app::BOARD_NAME;
  document_["dev_mode"] = "diagnostics";
  document_["reset_reason"] = static_cast<uint8_t>(esp_reset_reason());
  document_["flash_bytes"] = ESP.getFlashChipSize();
  document_["psram_bytes"] = ESP.getPsramSize();
  document_["cpu_mhz"] = ESP.getCpuFreqMHz();
  writeDocument();
}

void DevMonitor::emitSample(uint32_t nowMs, bool requested,
                            uint32_t requestId) {
  const HallSensorSnapshot hall = sensor_->snapshot();
  const RideStats& stats = ride_->stats();
  const TouchPoint& touchPoint = touch_->point();
  const PhoneState& phoneState = phone_->state();
  const SpeedTrendSnapshot& trend = speedTrend_->snapshot();
  const uint32_t intervalMs = max<uint32_t>(1, nowMs - intervalStartedMs_);

  document_.clear();
  document_["type"] = requested ? "snapshot" : "sample";
  document_["schema"] = 1;
  document_["seq"] = requested ? sequence_ : sequence_++;
  if (requested) document_["id"] = requestId;
  document_["uptime_ms"] = nowMs;

  JsonObject loop = document_.createNestedObject("loop");
  loop["hz"] = intervalLoopCount_ * 1000.0f / intervalMs;
  loop["last_period_us"] = lastLoopPeriodUs_;
  loop["max_period_us"] = maximumLoopPeriodUs_;
  loop["max_period_ever_us"] = maximumLoopPeriodEverUs_;
  loop["stack_high_water_words"] = uxTaskGetStackHighWaterMark(nullptr);

  JsonObject memory = document_.createNestedObject("memory");
  memory["heap_free"] = ESP.getFreeHeap();
  memory["heap_min"] = ESP.getMinFreeHeap();
  memory["heap_largest_block"] =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  memory["psram_free"] = ESP.getFreePsram();
  memory["psram_min"] = ESP.getMinFreePsram();

  JsonObject system = document_.createNestedObject("system");
  system["firmware"] = app::FIRMWARE_VERSION;
  system["board"] = app::BOARD_NAME;
  system["cpu_mhz"] = ESP.getCpuFreqMHz();
  system["temperature_c"] = temperatureRead();
  system["reset_reason"] = static_cast<uint8_t>(esp_reset_reason());
  system["dropped_documents"] = droppedDocuments_;

  JsonObject display = document_.createNestedObject("display");
  display["ready"] = display_->isReady();
  display["framebuffer"] = display_->frameBufferReady();
  display["transfer_buffer"] = display_->frameTransferBufferReady();
  display["brightness_percent"] = display_->brightness();

  JsonObject touch = document_.createNestedObject("touch");
  touch["ready"] = touch_->isReady();
  touch["touched"] = touchPoint.touched;
  touch["points"] = touchPoint.points;
  touch["int_level"] = touchPoint.intLevel;
  touch["x"] = touchPoint.x;
  touch["y"] = touchPoint.y;
  touch["last_touch_ms"] = touchPoint.lastTouchMs;
  touch["last_touch_age_ms"] =
      touchPoint.lastTouchMs ? nowMs - touchPoint.lastTouchMs : 0;
  JsonArray contacts = touch.createNestedArray("contacts");
  for (uint8_t i = 0; i < 2; ++i) {
    JsonObject contact = contacts.createNestedObject();
    contact["valid"] = touchPoint.contacts[i].valid;
    contact["id"] = touchPoint.contacts[i].id;
    contact["event"] = touchPoint.contacts[i].event;
    contact["raw_x"] = touchPoint.contacts[i].rawX;
    contact["raw_y"] = touchPoint.contacts[i].rawY;
    contact["x"] = touchPoint.contacts[i].x;
    contact["y"] = touchPoint.contacts[i].y;
  }

  JsonObject hallJson = document_.createNestedObject("hall");
  hallJson["level"] = hall.pinLevel;
  hallJson["pulses"] = hall.pulseCount;
  hallJson["rejected"] = hall.rejectedPulseCount;
  hallJson["last_pulse_us"] = hall.lastPulseUs;
  hallJson["last_interval_us"] = hall.lastIntervalUs;

  JsonObject speed = document_.createNestedObject("speed");
  speed["raw_kmh"] = speed_->rawKmh();
  speed["filtered_kmh"] = speed_->filteredKmh();
  speed["display_kmh"] = speed_->currentKmh();
  speed["since_pulse_us"] = speed_->timeSincePulseUs();
  JsonArray trendArray = speed.createNestedArray("trend");
  for (uint8_t i = 0; i < 3; ++i) {
    JsonObject item = trendArray.createNestedObject();
    item["window_ms"] = trend.readings[i].windowMs;
    item["ready"] = trend.readings[i].ready;
    item["delta_kmh"] = trend.readings[i].deltaKmh;
    item["state"] = static_cast<uint8_t>(trend.readings[i].state);
  }

  JsonObject ride = document_.createNestedObject("ride");
  ride["state"] = static_cast<uint8_t>(ride_->state());
  ride["motion"] = static_cast<uint8_t>(ride_->motionState());
  ride["distance_m"] = stats.distanceM;
  ride["max_kmh"] = stats.maxSpeedKmh;
  ride["avg_moving_kmh"] = stats.averageMovingSpeedKmh;
  ride["elapsed_ms"] = stats.elapsedMs;
  ride["recording_ms"] = stats.recordingMs;
  ride["moving_ms"] = stats.movingMs;
  ride["pause_ms"] = stats.pauseMs;
  ride["stopped_ms"] = stats.stoppedMs;

  JsonObject storage = document_.createNestedObject("storage");
  storage["sd_available"] = storage_->sdAvailable();
  storage["logging_enabled"] = storage_->loggingEnabled();
  storage["usb_owned"] = storage_->usbModeActive();
  storage["spi_hz"] = storage_->activeFrequencyHz();
  storage["logger_active"] = logger_->active();
  storage["ride_id"] = logger_->rideId();
  storage["sample_index"] = logger_->sampleIndex();
  storage["buffered_samples"] = logger_->bufferedCount();
  storage["logging_gap"] = logger_->loggingGap();

  JsonObject battery = document_.createNestedObject("battery");
  battery["enabled"] = battery_->enabled();
  battery["raw_adc"] = battery_->rawAdc();
  battery["adc_mv"] = battery_->adcMillivolts();
  battery["instant_v"] = battery_->instantVoltage();
  battery["filtered_v"] = battery_->voltage();
  battery["percent"] = battery_->percent();
  battery["state"] = static_cast<uint8_t>(battery_->state());
  battery["usb_data"] = battery_->usbConnected();
  battery["remaining_min"] = battery_->remainingMinutes();
  battery["runtime_quality"] =
      static_cast<uint8_t>(battery_->runtimeEstimateQuality());
  battery["runtime_observed_ms"] = battery_->runtimeObservedMs();
  battery["runtime_observed_drop"] = battery_->runtimeObservedDrop();

  JsonObject phone = document_.createNestedObject("phone");
  phone["link"] = static_cast<uint8_t>(phoneState.link);
  phone["ready"] = phone_->ready();
  phone["paired"] = phoneState.paired;
  phone["authorized"] = phoneState.authorized;
  phone["mtu"] = phoneState.negotiatedMtu;
  phone["protocol"] = phoneState.protocolVersion;
  phone["capabilities"] = phoneState.capabilities;
  phone["media_supported"] = phone_->mediaSupported();
  phone["remembered"] = phone_->rememberedPhoneCount();
  phone["clock_synced"] = phone_->clock().synced();
  phone["last_error"] = phoneState.lastError;

  const media::MediaState& mediaState = phone_->mediaState();
  JsonObject mediaJson = document_.createNestedObject("media");
  mediaJson["available"] = mediaState.available;
  mediaJson["playing"] = mediaState.playing;
  mediaJson["supported_actions"] = mediaState.supportedActions;
  mediaJson["duration_ms"] = mediaState.durationMs;
  mediaJson["position_ms"] = mediaState.positionNow(nowMs);
  mediaJson["received_age_ms"] =
      mediaState.receivedAtMs ? nowMs - mediaState.receivedAtMs : 0;
  mediaJson["player"] = mediaState.player;
  mediaJson["title"] = mediaState.title;
  mediaJson["artist"] = mediaState.artist;

  const navigation::NavigationState& navigationState =
      phone_->navigationState();
  JsonObject navigationJson = document_.createNestedObject("navigation");
  navigationJson["available"] = navigationState.available;
  navigationJson["lifecycle"] =
      static_cast<uint8_t>(navigationState.lifecycle);
  navigationJson["maneuver"] =
      static_cast<uint8_t>(navigationState.maneuver);
  navigationJson["next_maneuver"] =
      static_cast<uint8_t>(navigationState.nextManeuver);
  navigationJson["distance_to_maneuver_m"] =
      navigationState.distanceToManeuverM;
  navigationJson["next_distance_m"] = navigationState.nextDistanceM;
  navigationJson["remaining_distance_m"] =
      navigationState.remainingDistanceM;
  navigationJson["eta_utc_ms"] = navigationState.etaUtcMs;
  navigationJson["street"] = navigationState.street;

  const phonegeo::LocationState& locationState = phone_->locationState();
  const phonegeo::LocationFix& locationFix = locationState.fix;
  JsonObject locationJson = document_.createNestedObject("location");
  locationJson["available"] = locationFix.available;
  locationJson["fresh"] = locationFix.fresh(nowMs);
  locationJson["accepted"] = locationState.acceptedCount;
  locationJson["rejected"] = locationState.rejectedCount;
  if (locationFix.available) {
    locationJson["ride_id"] = locationFix.rideId;
    locationJson["timestamp_utc_ms"] = locationFix.timestampUtcMs;
    locationJson["latitude"] = locationFix.latitude();
    locationJson["longitude"] = locationFix.longitude();
    locationJson["age_ms"] = nowMs - locationFix.receivedAtMs;
    if ((locationFix.flags & phonegeo::FixFlags::HasAltitude) != 0) {
      locationJson["altitude_m"] = locationFix.altitudeM();
    } else {
      locationJson["altitude_m"] = nullptr;
    }
    if ((locationFix.flags & phonegeo::FixFlags::HasAccuracy) != 0) {
      locationJson["accuracy_m"] = locationFix.accuracyM();
    } else {
      locationJson["accuracy_m"] = nullptr;
    }
    if ((locationFix.flags & phonegeo::FixFlags::HasSpeed) != 0) {
      locationJson["speed_mps"] = locationFix.speedMps();
    } else {
      locationJson["speed_mps"] = nullptr;
    }
  } else {
    locationJson["ride_id"] = 0;
    locationJson["timestamp_utc_ms"] = nullptr;
    locationJson["latitude"] = nullptr;
    locationJson["longitude"] = nullptr;
    locationJson["age_ms"] = nullptr;
    locationJson["altitude_m"] = nullptr;
    locationJson["accuracy_m"] = nullptr;
    locationJson["speed_mps"] = nullptr;
  }

  JsonObject ui = document_.createNestedObject("ui");
  ui["screen"] = ui_->currentScreenId();
  ui["rain_state"] = ui_->rainLockStateId();
  ui["rain_confirm"] = ui_->rainConfirmVisible();

  JsonObject usb = document_.createNestedObject("usb");
  usb["data_connected"] = usb_->dataConnected();
  usb["msc_active"] = usb_->active();

  document_["ride_sync_state"] = static_cast<uint8_t>(rideSync_->state());
  document_["config_revision"] = configSync_->revision();
  writeDocument();
}

void DevMonitor::processInput(uint32_t nowMs) {
  // Bound RX work so a pasted command burst cannot starve Hall/UI servicing.
  for (uint16_t budget = 0; budget < 192; ++budget) {
    if (g_devUsb.available() <= 0) return;
    const int value = g_devUsb.read();
    if (value < 0) return;
    const char ch = static_cast<char>(value);
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (inputOverflow_) {
        sendErrorResponse(0, "parse", "command_too_long");
      } else if (inputLength_ != 0) {
        input_[inputLength_] = '\0';
        handleCommand(input_, inputLength_, nowMs);
      }
      inputLength_ = 0;
      inputOverflow_ = false;
      continue;
    }
    if (inputOverflow_) continue;
    if (inputLength_ + 1 >= sizeof(input_)) {
      inputOverflow_ = true;
      continue;
    }
    input_[inputLength_++] = ch;
  }
}

void DevMonitor::beginResponse(uint32_t requestId, const char* command,
                               bool ok) {
  document_.clear();
  document_["type"] = "response";
  document_["schema"] = 1;
  document_["id"] = requestId;
  document_["cmd"] = command;
  document_["ok"] = ok;
}

void DevMonitor::sendErrorResponse(uint32_t requestId, const char* command,
                                   const char* error) {
  beginResponse(requestId, command, false);
  document_["error"] = error;
  writeDocument();
}

void DevMonitor::handleCommand(const char* input, size_t length,
                               uint32_t nowMs) {
  while (length && (*input == ' ' || *input == '\t')) {
    ++input;
    --length;
  }
  if (length > 4 && strncmp(input, "DEV ", 4) == 0) {
    input += 4;
    length -= 4;
  }

  commandDocument_.clear();
  const DeserializationError parseError =
      deserializeJson(commandDocument_, input, length);
  if (parseError || !commandDocument_.is<JsonObject>()) {
    sendErrorResponse(0, "parse", parseError ? parseError.c_str()
                                               : "object_required");
    return;
  }

  const uint32_t requestId = commandDocument_["id"] | 0U;
  const char* command = commandDocument_["cmd"] | "";
  if (!command[0]) {
    sendErrorResponse(requestId, "parse", "cmd_required");
    return;
  }

  if (strcmp(command, "ping") == 0) {
    beginResponse(requestId, command, true);
    document_["uptime_ms"] = nowMs;
    document_["firmware"] = app::FIRMWARE_VERSION;
    writeDocument();
    return;
  }

  if (strcmp(command, "help") == 0) {
    beginResponse(requestId, command, true);
    document_["transport"] = "native USB Serial/JTAG JSONL";
    JsonArray commands = document_.createNestedArray("commands");
    commands.add("ping");
    commands.add("help");
    commands.add("snapshot");
    commands.add("self_test");
    commands.add("stream");
    commands.add("preview");
    commands.add("sd_test");
    commands.add("rgb");
    commands.add("media_action");
    commands.add("ride_control");
    commands.add("usb_storage");
    document_["screen_count"] =
        static_cast<uint16_t>(ui_exact::ScreenId::COUNT);
    writeDocument();
    return;
  }

  if (strcmp(command, "snapshot") == 0) {
    emitSample(nowMs, true, requestId);
    return;
  }

  if (strcmp(command, "stream") == 0) {
    if (commandDocument_.containsKey("interval_ms")) {
      const uint32_t interval = commandDocument_["interval_ms"] | 0U;
      if (interval < kMinimumReportIntervalMs ||
          interval > kMaximumReportIntervalMs) {
        sendErrorResponse(requestId, command, "interval_out_of_range");
        return;
      }
      reportIntervalMs_ = interval;
    }
    if (commandDocument_.containsKey("enabled")) {
      streamEnabled_ = commandDocument_["enabled"].as<bool>();
    }
    beginResponse(requestId, command, true);
    document_["enabled"] = streamEnabled_;
    document_["interval_ms"] = reportIntervalMs_;
    writeDocument();
    return;
  }

  if (strcmp(command, "self_test") == 0) {
    const HallSensorSnapshot hall = sensor_->snapshot();
    const PhoneState& phoneState = phone_->state();
    beginResponse(requestId, command, true);
    JsonObject checks = document_.createNestedObject("checks");
    checks["display"] = display_->isReady();
    checks["framebuffer"] = display_->frameBufferReady();
    checks["touch"] = touch_->isReady();
    checks["hall_level"] = hall.pinLevel;
    checks["battery"] = battery_->enabled();
    checks["sd"] = storage_->sdAvailable();
    checks["ble_ready"] = phone_->ready();
    checks["ble_authorized"] = phoneState.authorized;
    checks["media_available"] = phone_->mediaState().available;
    checks["navigation_available"] = phone_->navigationState().available;
    checks["location_available"] = phone_->locationState().fix.available;
    checks["location_fresh"] = phone_->locationState().fix.fresh(millis());
    checks["usb_msc"] = usb_->active();
    document_["core_ok"] = display_->isReady() &&
                            display_->frameBufferReady() && battery_->enabled();
    document_["note"] =
        "SD, touch, phone, media and navigation are optional live checks";
    writeDocument();
    return;
  }

  if (strcmp(command, "preview") == 0) {
    const int32_t screen = commandDocument_["screen"] | -1;
    if (screen < 0) {
      ui_->clearDevPreview();
      beginResponse(requestId, command, true);
      document_["active"] = false;
      writeDocument();
      return;
    }
    if (!ui_->setDevPreview(static_cast<uint16_t>(screen))) {
      sendErrorResponse(requestId, command, "invalid_screen");
      return;
    }
    beginResponse(requestId, command, true);
    document_["active"] = true;
    document_["screen"] = screen;
    document_["name"] = ui_exact::getScreenAsset(
                            static_cast<ui_exact::ScreenId>(screen))
                            .name;
    writeDocument();
    return;
  }

  if (strcmp(command, "sd_test") == 0) {
    if (!storage_->sdAvailable()) {
      sendErrorResponse(requestId, command, "sd_unavailable");
      return;
    }
    if (logger_->active() || ride_->state() == RideState::RIDING ||
        usb_->active()) {
      sendErrorResponse(requestId, command, "storage_busy");
      return;
    }
    const SdTestResult result = storage_->runSdTest();
    beginResponse(requestId, command, result.ok);
    document_["message"] = result.message;
    document_["read_back"] = result.readBack;
    writeDocument();
    return;
  }

  if (strcmp(command, "rgb") == 0) {
    if (commandDocument_["clear"] | false) {
      speedTrend_->clearDiagnosticRgb();
      beginResponse(requestId, command, true);
      document_["active"] = false;
      writeDocument();
      return;
    }
    const int red = constrain(commandDocument_["r"] | 0, 0, 255);
    const int green = constrain(commandDocument_["g"] | 0, 0, 255);
    const int blue = constrain(commandDocument_["b"] | 0, 0, 255);
    speedTrend_->setDiagnosticRgb(red, green, blue);
    beginResponse(requestId, command, true);
    document_["active"] = true;
    document_["r"] = red;
    document_["g"] = green;
    document_["b"] = blue;
    writeDocument();
    return;
  }

  if (strcmp(command, "media_action") == 0) {
    const char* actionName = commandDocument_["action"] | "";
    media::Action action;
    if (strcmp(actionName, "play") == 0) action = media::Action::Play;
    else if (strcmp(actionName, "pause") == 0) action = media::Action::Pause;
    else if (strcmp(actionName, "toggle") == 0) action = media::Action::Toggle;
    else if (strcmp(actionName, "next") == 0) action = media::Action::Next;
    else if (strcmp(actionName, "previous") == 0) action = media::Action::Previous;
    else if (strcmp(actionName, "seek") == 0) action = media::Action::Seek;
    else {
      sendErrorResponse(requestId, command, "invalid_action");
      return;
    }
    const uint64_t positionMs = commandDocument_["position_ms"] | 0ULL;
    const bool sent = phone_->sendMediaAction(action, positionMs);
    if (!sent) {
      sendErrorResponse(requestId, command,
                        "media_unavailable_or_phone_disconnected");
      return;
    }
    beginResponse(requestId, command, true);
    document_["action"] = actionName;
    document_["position_ms"] = positionMs;
    writeDocument();
    return;
  }

  if (strcmp(command, "ride_control") == 0) {
    const char* action = commandDocument_["action"] | "";
    String error;
    if (!ui_->handleDevRideAction(action, error)) {
      sendErrorResponse(requestId, command,
                        error.length() ? error.c_str() : "ride_action_failed");
      return;
    }
    beginResponse(requestId, command, true);
    document_["action"] = action;
    document_["ride_state"] = static_cast<uint8_t>(ride_->state());
    document_["ride_id"] = logger_->rideId();
    writeDocument();
    return;
  }

  if (strcmp(command, "usb_storage") == 0) {
    if (!(commandDocument_["confirm"] | false)) {
      sendErrorResponse(requestId, command, "confirmation_required");
      return;
    }
    if (!storage_->sdAvailable()) {
      sendErrorResponse(requestId, command, "sd_unavailable");
      return;
    }
    if (ride_->state() == RideState::RIDING || usb_->active() ||
        usbStoragePending_) {
      sendErrorResponse(requestId, command, "storage_busy");
      return;
    }
    beginResponse(requestId, command, true);
    document_["takeover_in_ms"] = 750;
    document_["note"] = "Dev USB closes; safe eject and reboot to exit MSC";
    writeDocument();
    usbStoragePending_ = true;
    usbStorageRequestedMs_ = nowMs;
    return;
  }

  sendErrorResponse(requestId, command, "unknown_command");
}

void DevMonitor::writeDocument() {
  if (document_.overflowed()) {
    ++droppedDocuments_;
    g_devUsb.println("DEV {\"type\":\"error\",\"error\":\"document_overflow\"}");
    return;
  }
  const size_t length = serializeJson(document_, line_, sizeof(line_));
  if (length == 0 || length + 1 >= sizeof(line_)) {
    ++droppedDocuments_;
    g_devUsb.println("DEV {\"type\":\"error\",\"error\":\"serialization_failed\"}");
    return;
  }
  g_devUsb.print("DEV ");
  g_devUsb.write(reinterpret_cast<const uint8_t*>(line_), length);
  g_devUsb.write('\n');
}
