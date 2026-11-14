/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */

#include "setting.h"
#include "config.h"

RF24 RadioA(NRF_CE_PIN_A, NRF_CSN_PIN_A);
RF24 RadioB(NRF_CE_PIN_B, NRF_CSN_PIN_B);
RF24 RadioC(NRF_CE_PIN_C, NRF_CSN_PIN_C);

void setRadiosNeutralState() {
  RadioA.stopListening();
  RadioA.setAutoAck(false);
  RadioA.setRetries(0, 0);
  RadioA.powerDown(); 
  digitalWrite(NRF_CE_PIN_A, LOW); 

  RadioB.stopListening();
  RadioB.setAutoAck(false);
  RadioB.setRetries(0, 0);
  RadioB.powerDown(); 
  digitalWrite(NRF_CE_PIN_B, LOW); 

  RadioC.stopListening();
  RadioC.setAutoAck(false);
  RadioC.setRetries(0, 0);
  RadioC.powerDown(); 
  digitalWrite(NRF_CE_PIN_C, LOW); 
}

void configureNrf(RF24 &radio) {
  radio.begin();
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
}

void setupRadioA() {
  configureNrf(RadioA);
}

void setupRadioB() {
  configureNrf(RadioB);
}

void setupRadioC() {
  configureNrf(RadioC);
}

void initAllRadios() {
  setupRadioA();
  setupRadioB();
  setupRadioC();
}

// Wait while blinking the NeoPixel yellow (boot indicator).
static void splashWait(unsigned long ms) {
  unsigned long start = millis();
  bool on = false;
  while (millis() - start < ms) {
    on = !on;
    setNeoPixelColour(on ? "yellow" : "null");
    delay(150);
  }
}

void conf() {
  u8g2.setBitmapMode(1);
  // Slide the logo in from the right edge.
  for (int x = 128; x > 0; x -= 8) {
    u8g2.clearBuffer();
    u8g2.drawXBMP(x, 0, 128, 64, splashLogo);
    u8g2.sendBuffer();
    setNeoPixelColour(((x / 8) & 1) ? "yellow" : "null");
    delay(15);
  }
  u8g2.clearBuffer();
  u8g2.drawXBMP(0, 0, 128, 64, splashLogo);
  u8g2.sendBuffer();
  splashWait(2500);

  u8g2.clearBuffer();
  const char* title = "RF Clown X";
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.setCursor((128 - u8g2.getUTF8Width(title)) / 2, 22);
  u8g2.print(title);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor((128 - u8g2.getUTF8Width(FW_VERSION)) / 2, 44);
  u8g2.print(FW_VERSION);
  const char* forkline = "fork of RF-Clown";
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor((128 - u8g2.getUTF8Width(forkline)) / 2, 56);
  u8g2.print(forkline);
  u8g2.sendBuffer();
  splashWait(2000);
}

 
