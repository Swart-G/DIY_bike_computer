# Hardware contract

Плата: ESP32-S3-N16R8, 16 MB Flash, 8 MB PSRAM, Arduino/PlatformIO. Любая прошивка обязана использовать только определения из `src/config/hardware_config.h`.

| GPIO | Назначение |
|---:|---|
| 4 | Hall sensor signal, configurable pull-up/edge; default LOW/FALLING |
| 6 | Battery ADC1, 1S Li-Po divider |
| 8 | FT6336 SDA |
| 9 | ST7796 DC |
| 10 | ST7796 CS |
| 11 | Shared SPI MOSI |
| 12 | Shared SPI SCK |
| 13 | Shared SPI MISO |
| 14 | ST7796 RESET |
| 15 | SD CS |
| 16 | FT6336 INT |
| 17 | FT6336 RESET |
| 18 | FT6336 SCL |
| 19 | Native USB D− — never use as GPIO |
| 20 | Native USB D+ — never use as GPIO |
| 47 | TFT backlight PWM |
| 48 | Built-in addressable RGB LED |

TFT is ST7796, 480×320 landscape, shared SPI at 20 MHz with 10 MHz register reads.
GPIO13/MISO remains connected to both the TFT and SD socket. SD uses the same bus with
its own CS and mounts first at 10 MHz, with one ordinary 1 MHz fallback attempt. The
proven 1.0 ownership model serializes every transaction with a recursive mutex, keeps
TFT CS and SD CS mutually exclusive, and parks ST7796 in RAM-write mode before selecting
the card. Firmware does not issue raw SD commands, restart the SPI peripheral, hold the
display in reset, background-probe the panel, automatically remount after an I/O error,
or format after a mount failure. FT6336 I²C address is `0x38`; the production bus
runs at 100 kHz for FPC noise margin, and mapping remains configurable through
swap/invert constants.

If SD mount or I/O errors remain, firmware cannot power-cycle the card because SD power
has no controllable GPIO. The production wiring should provide a physical 10 kΩ pull-up
from SD CS/GPIO15 to 3.3 V, 100 nF ceramic plus 10–47 µF low-ESR decoupling directly at
the SD socket VCC/GND, short SCK/MOSI/MISO/CS conductors and a solid common ground.
For persistent failures, verify the 3.3 V rail at the socket under BLE/display load and
test a known-good FAT32 card. Do not add an automatic formatter or repurpose another
GPIO as an invented SD power switch.

Battery divider is `BAT+ — 1 MΩ — node — 1 MΩ — GND`, with a 100 kΩ series resistor from node to GPIO6. Nominal ratio is 2.0: 4.2 V battery yields about 2.1 V ADC. There is no capacitor, so firmware intentionally discards initial samples and uses a distributed median/trimmed series. Never feed 5 V into ESP32 GPIO, TFT VCC or Hall signal. TFT and Hall are powered from 3.3 V; MH-CD42 OUT-5V goes only to ESP32 VIN through the physical switch.

The external Type-C data connector is native ESP32-S3 USB. A USB connection alone is not a charge signal; charging is inferred only from a slow voltage trend.

The built-in RGB LED is the single-wire addressable LED declared by the `esp32s3`
Arduino variant on GPIO48. Firmware writes it from the main loop only when colour or
brightness changes; no LED work is performed by the Hall ISR.
