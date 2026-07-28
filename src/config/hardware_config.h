#pragma once

#include <Arduino.h>

namespace hw {

static constexpr int PIN_HALL_SENSOR = 4;
static constexpr int PIN_BATTERY_ADC = 6;
static constexpr int PIN_CTP_SDA = 8;
static constexpr int PIN_LCD_DC = 9;
static constexpr int PIN_LCD_CS = 10;
static constexpr int PIN_SPI_MOSI = 11;
static constexpr int PIN_SPI_SCK = 12;
static constexpr int PIN_SPI_MISO = 13;
static constexpr int PIN_LCD_RST = 14;
static constexpr int PIN_SD_CS = 15;
static constexpr int PIN_CTP_INT = 16;
static constexpr int PIN_CTP_RST = 17;
static constexpr int PIN_CTP_SCL = 18;
static constexpr int PIN_USB_DM = 19;
static constexpr int PIN_USB_DP = 20;
static constexpr int PIN_LCD_BACKLIGHT = 47;
static constexpr bool LCD_BACKLIGHT_ACTIVE_HIGH = true;
static constexpr uint8_t LCD_BACKLIGHT_PWM_CHANNEL = 7;
static constexpr uint32_t LCD_BACKLIGHT_PWM_FREQUENCY_HZ = 200;
static constexpr uint8_t LCD_BACKLIGHT_PWM_RESOLUTION_BITS = 8;

static constexpr bool BATTERY_MONITOR_ENABLED = true;
static constexpr float BATTERY_VOLTAGE_DIVIDER_RATIO = 2.0f;
static constexpr uint8_t BATTERY_DISCARD_SAMPLES = 3;
static constexpr uint8_t BATTERY_SERIES_SAMPLES = 7;
static constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 25;
static constexpr uint32_t BATTERY_REPORT_INTERVAL_MS = 2000;
static constexpr float BATTERY_CHARGE_DELTA_V = 0.015f;
static constexpr float BATTERY_FULL_VOLTAGE = 4.18f;

static constexpr uint32_t TFT_SPI_FREQUENCY_HZ = 20000000UL;
static constexpr uint32_t SD_SPI_FREQUENCY_HZ = 10000000UL;
static constexpr uint32_t TOUCH_I2C_FREQUENCY_HZ = 400000UL;

static constexpr uint8_t FT6336_I2C_ADDRESS = 0x38;
static constexpr uint8_t DISPLAY_ROTATION = 1;
static constexpr bool DISPLAY_INVERT_COLORS = true;
static constexpr int16_t DISPLAY_WIDTH = 480;
static constexpr int16_t DISPLAY_HEIGHT = 320;

// Touch mapping for landscape rotation. Adjust these only after raw touch test.
static constexpr bool TOUCH_SWAP_XY = true;
static constexpr bool TOUCH_INVERT_X = false;
static constexpr bool TOUCH_INVERT_Y = true;
static constexpr int16_t TOUCH_RAW_WIDTH = 320;
static constexpr int16_t TOUCH_RAW_HEIGHT = 480;

}  // namespace hw
