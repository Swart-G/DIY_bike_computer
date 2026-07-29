#include "bus/SharedSpiBus.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <TFT_eSPI.h>

#include "config/hardware_config.h"

namespace {

SemaphoreHandle_t sharedSpiMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
  return mutex;
}

void tftWriteCommand(SPIClass& spi, uint8_t command) {
  digitalWrite(hw::PIN_LCD_DC, LOW);
  spi.write(command);
}

void tftWriteData(SPIClass& spi, const uint8_t* data, size_t len) {
  digitalWrite(hw::PIN_LCD_DC, HIGH);
  spi.writeBytes(data, len);
}

}  // namespace

namespace hw {

void lockSharedSpiBus() {
  SemaphoreHandle_t mutex = sharedSpiMutex();
  if (mutex != nullptr) {
    xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
  }
}

void unlockSharedSpiBus() {
  SemaphoreHandle_t mutex = sharedSpiMutex();
  if (mutex != nullptr) {
    xSemaphoreGiveRecursive(mutex);
  }
}

void releaseSharedSpiDevices() {
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  delayMicroseconds(2);
}

void parkDisplayForSharedSdTraffic() {
  SPIClass& spi = TFT_eSPI::getSPIinstance();
  SPISettings settings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE);

  digitalWrite(PIN_SD_CS, HIGH);
  digitalWrite(PIN_LCD_CS, HIGH);
  delayMicroseconds(2);

  spi.beginTransaction(settings);
  digitalWrite(PIN_LCD_CS, LOW);

  const uint8_t column[] = {0x00, 0x00, 0x00, 0x00};
  const uint8_t row[] = {0x00, 0x00, 0x00, 0x00};
  const uint8_t black[] = {0x00, 0x00};

  tftWriteCommand(spi, 0x2A);
  tftWriteData(spi, column, sizeof(column));
  tftWriteCommand(spi, 0x2B);
  tftWriteData(spi, row, sizeof(row));
  tftWriteCommand(spi, 0x2C);
  tftWriteData(spi, black, sizeof(black));

  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_LCD_DC, HIGH);
  spi.endTransaction();
  delayMicroseconds(2);
}

SharedSpiBusGuard::SharedSpiBusGuard(bool parkDisplayForSd) {
  SemaphoreHandle_t mutex = sharedSpiMutex();
  if (mutex != nullptr) {
    locked_ = xSemaphoreTakeRecursive(mutex, portMAX_DELAY) == pdTRUE;
  }
  if (parkDisplayForSd) {
    parkDisplayForSharedSdTraffic();
    return;
  }
  releaseSharedSpiDevices();
}

SharedSpiBusGuard::~SharedSpiBusGuard() {
  releaseSharedSpiDevices();
  if (locked_) {
    xSemaphoreGiveRecursive(sharedSpiMutex());
  }
}

}  // namespace hw
