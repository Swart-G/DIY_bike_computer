#include "bus/SharedSpiBus.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <TFT_eSPI.h>
#include <driver/gpio.h>

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

void configureSharedSpiChipSelects() {
  pinMode(PIN_LCD_CS, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  gpio_set_drive_capability(static_cast<gpio_num_t>(PIN_LCD_CS),
                            GPIO_DRIVE_CAP_3);
  gpio_set_drive_capability(static_cast<gpio_num_t>(PIN_SD_CS),
                            GPIO_DRIVE_CAP_3);
  gpio_pullup_en(static_cast<gpio_num_t>(PIN_LCD_CS));
  gpio_pullup_en(static_cast<gpio_num_t>(PIN_SD_CS));
  delayMicroseconds(2);
}

void releaseSharedSpiDevices() {
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  delayMicroseconds(2);
}

void clockSdCardIdle(uint16_t byteCount) {
  SPIClass& spi = TFT_eSPI::getSPIinstance();
  SPISettings idleSettings(400000UL, MSBFIRST, SPI_MODE0);
  spi.beginTransaction(idleSettings);
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  for (uint16_t i = 0; i < byteCount; ++i) {
    spi.write(0xFF);
  }
  spi.endTransaction();
}

bool abortSdTransfer() {
  SPIClass& spi = TFT_eSPI::getSPIinstance();
  SPISettings settings(400000UL, MSBFIRST, SPI_MODE0);
  uint8_t cmd12[] = {
      0x40U | 12U, 0x00, 0x00, 0x00, 0x00, 0x61,
  };

  releaseSharedSpiDevices();
  clockSdCardIdle(2);
  spi.beginTransaction(settings);
  digitalWrite(PIN_LCD_CS, HIGH);
  digitalWrite(PIN_SD_CS, LOW);
  spi.writeBytes(cmd12, sizeof(cmd12));

  // CMD12 has one mandatory stuff byte before R1. If the card was in a
  // multi-block read/write state, it may then hold DO low while completing
  // the stopped transfer.
  spi.transfer(0xFF);
  uint8_t response = 0xFF;
  for (uint8_t i = 0; i < 16; ++i) {
    response = spi.transfer(0xFF);
    if ((response & 0x80U) == 0) {
      break;
    }
  }

  const uint32_t startedMs = millis();
  while (spi.transfer(0xFF) == 0x00 &&
         millis() - startedMs < 1200) {
    delay(1);
  }
  digitalWrite(PIN_SD_CS, HIGH);
  spi.transfer(0xFF);
  spi.endTransaction();
  delayMicroseconds(50);
  return response != 0xFF;
}

void parkDisplayForSharedSdTraffic() {
  SPIClass& spi = TFT_eSPI::getSPIinstance();
  SPISettings settings(400000UL, MSBFIRST, TFT_SPI_MODE);

  releaseSharedSpiDevices();
  spi.beginTransaction(settings);
  digitalWrite(PIN_LCD_CS, LOW);

  // A register read can leave some ST7796 boards driving SDO until the
  // controller sees a write command. Park it in RAM-write mode at the SD init
  // frequency, not at the 20 MHz display rate, to avoid a sharp transition on
  // the shared harness immediately before selecting the card.
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
  clockSdCardIdle(2);
  delayMicroseconds(50);
}

SharedSpiBusGuard::SharedSpiBusGuard(bool parkDisplayForSd) {
  sdTraffic_ = parkDisplayForSd;
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
  if (sdTraffic_) {
    // SPI-mode SD cards require at least eight clocks with CS high to finish
    // releasing DO after a transaction. Without this boundary, the next TFT
    // transfer can leave some cards in STA_NOINIT and the following fopen()
    // reports ENODEV. Send two idle bytes before handing the bus back.
    clockSdCardIdle(2);
    delayMicroseconds(50);
  }
  if (locked_) {
    xSemaphoreGiveRecursive(sharedSpiMutex());
  }
}

}  // namespace hw
