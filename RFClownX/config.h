/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */

#ifndef CONFIG_H
#define CONFIG_H

// Single source of truth for the firmware version (splash + header + System).
#define FW_VERSION "v2.2.0"

#define SCREEN_W 128
#define SCREEN_H 64

// ---------------- Pins ----------------
#define PIN_BTN_L  27
#define PIN_BTN_R  25
#define PIN_BTN_S  26

// NeoPixel status LED (GPIO 4 is taken by RadioB CSN, so use 14)
#define PIN_NEOPIXEL 14

// nRF24-specific Pins
#define NRF_CE_PIN_A    5   
#define NRF_CSN_PIN_A   17 
#define NRF_CE_PIN_B    16  
#define NRF_CSN_PIN_B   4  
#define NRF_CE_PIN_C    15  
#define NRF_CSN_PIN_C   2  

// Common dependencies
#include "setting.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <string>
#include <math.h>

inline U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

inline Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// BLE-specific dependencies
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>

// nRF24-specific dependencies
#include <nRF24L01.h>
#include <RF24.h>

// WiFi-specific dependencies
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_system.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <string>

// ESP-specific configurations
#include <esp_bt.h>
#include <esp_wifi.h>

// External declarations
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
extern Adafruit_NeoPixel pixels;

static const uint8_t* FONT_SMALL  = u8g2_font_5x8_tf;
static const uint8_t* FONT_MEDIUM = u8g2_font_6x12_tf;
static const uint8_t* FONT_ICON_FALLBACK = u8g2_font_open_iconic_thing_2x_t;

const byte bluetooth_channels[] =        {32, 34, 46, 48, 50, 52, 0, 1, 2, 4, 6, 8, 22, 24, 26, 28, 30, 74, 76, 78, 80};
const byte ble_channels[]       =        {2, 26, 80};
// WiFi occupies ~2401-2484 MHz -> NRF24 channels 1-84. WiFi ch1=NRF12, ch6=NRF37,
// ch11=NRF62, ch14=NRF84. Every channel 1..84 (step 1) so manual single-channel
// selection can reach any 1 MHz slot (e.g. 2.417 GHz = NRF24 ch 17).
const byte WiFi_channels[]      =        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84};
const byte usbWireless_channels[] =      {40, 50, 60};
const byte videoTransmitter_channels[] = {70, 75, 80};
const byte rc_channels[]        =        {1, 3, 5, 7};
const byte zigbee_channels[]    =        {11, 15, 20, 25};
const byte nrf24_channels[]     =        {76, 78, 79};

inline volatile bool ChangeRequested  = false;
inline volatile bool ChangeRequested1 = false;
inline volatile bool ChangeRequested2 = false;
inline volatile bool SelectRequested  = false;   // short click
inline volatile bool SelectBack       = false;   // long press (back/exit)
inline volatile unsigned long selDownTime = 0;

inline unsigned long lastPressL = 0;   // per-button debounce timestamps
inline unsigned long lastPressR = 0;
inline const unsigned long debounceDelay = 200;
inline unsigned long longPressMs = 500;   // configurable back-button hold time (NVS)

// Remote WebUI bridge + service API (optional, see webui.cpp)
#include "webui.h"

#endif // CONFIG_H
