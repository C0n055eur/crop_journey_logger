#pragma once

// Remote (in-truck) node pin map. See docs/DEMO.md for the wiring table.

#define PIN_GPS_RX 16  // NEO-6M TX -> ESP32 UART1 RX
#define PIN_GPS_SIM_TX 17  // simulation only: NMEA generator, looped back to 16.
                           // On real hardware this pin is LoRa RST instead.

#define PIN_DHT 4
#define DHT_MODEL DHT22

#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

// Shared VSPI bus: microSD and the RFID reader, separate chip selects.
#define PIN_SPI_SCK 18
#define PIN_SPI_MISO 19
#define PIN_SPI_MOSI 23
#define PIN_SD_CS 5
#define PIN_RFID_CS 15
#define PIN_RFID_RST 13

#define PIN_LED_GREEN 25
#define PIN_LED_YELLOW 26
#define PIN_LED_RED 27

#define PIN_BTN_NEXT 32  // cycle crop / cycle nothing once running
#define PIN_BTN_OK 33    // confirm crop, then end-of-journey summary
