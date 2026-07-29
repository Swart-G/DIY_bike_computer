#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "speed/RideStateMachine.h"
#include "storage/RideRepository.h"
#include "ui/components/UiComponents.h"

namespace ui {

class SecondaryScreens {
 public:
  static void history(TFT_eSPI& tft, const HeaderStatus& header,
                      const RideSummaryItem* rides, uint8_t count,
                      uint8_t scrollOffset, const String& message);
  static void historyDetail(TFT_eSPI& tft, const HeaderStatus& header,
                            const RideSummaryItem* ride,
                            bool storageAvailable);
  static void deleteRideConfirm(TFT_eSPI& tft);
  static void diagnostics(TFT_eSPI& tft, const HeaderStatus& header,
                          bool sdAvailable);
  static void finishConfirm(TFT_eSPI& tft);
  static void rideSummary(TFT_eSPI& tft, const HeaderStatus& header,
                          const RideStats& stats);

 private:
  static String duration(uint64_t ms);
};

}  // namespace ui
