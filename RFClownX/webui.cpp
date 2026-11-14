/* ____________________________
   Remote WebUI for RF Clown X - service implementation
   Runs the HTTP server on its own task (core 0) so the blocking RF tools on
   core 1 never starve it. The web task never touches u8g2 or the radios: it
   only reads published telemetry and posts requests into the main task.
   ________________________________________ */

#include "config.h"
#include "webui.h"

#include <WebServer.h>
#include "webui_page.h"

static WebServer server(80);

static volatile bool gEnabled     = false;
static volatile bool gDesired     = true;
static volatile bool gTaskStarted = false;
static char gToken[9] = {0};

static volatile bool          gToolActive    = false;
static volatile unsigned long gToolStart     = 0;
static volatile uint32_t      gAutostopMs    = 45000;
static char                   gToolLabel[24] = {0};
static volatile bool          gFailsafeFired = false;
static volatile bool          gRestartServer = false;
static volatile bool          gRestoreAp     = false;

// OLED frame published by the main task, consumed by the web task.
static portMUX_TYPE gFrameMux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t      gFrame[1024];

// One-slot launch queue: web task writes, main task consumes.
static portMUX_TYPE  gExecMux  = portMUX_INITIALIZER_UNLOCKED;
static volatile int  gPendCat  = -1;
static volatile int  gPendItem = -1;

// ---------------------------------------------------------------- helpers ---

static bool authed() {
  if (server.method() != HTTP_POST) return true;
  if (!server.hasHeader("X-Token")) return false;
  return server.header("X-Token") == gToken;
}

static void deny() { server.send(401, "text/plain", "unauthorized"); }

static void bringApUp() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("RF_CLOWN_X", "clownx2400", 1, 0, 4);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
}

// ---------------------------------------------------------------- handlers ---

static void hRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html; charset=utf-8", WEBUI_HTML);
}

static void hStatus() {
  int lvl = 0, cat = 0, item = 0;
  webMenuState(lvl, cat, item);

  unsigned long left = 0;
  if (gToolActive && gAutostopMs) {
    unsigned long elapsed = millis() - gToolStart;
    if (elapsed < gAutostopMs) left = (gAutostopMs - elapsed + 999) / 1000;
  }

  String j;
  j.reserve(352);
  j += F("{\"v\":\"");      j += FW_VERSION;
  j += F("\",\"up\":");     j += (unsigned long)(millis() / 1000);
  j += F(",\"heap\":");     j += ESP.getFreeHeap();
  float t = temperatureRead();
  if (isnan(t)) t = 0;                      // sensor failure must not emit raw NaN
  j += F(",\"temp\":");     j += String(t, 1);
  j += F(",\"tool\":\"");   j += gToolLabel; j += '"';
  j += F(",\"act\":");      j += gToolActive ? 1 : 0;
  j += F(",\"as\":");       j += (unsigned long)gAutostopMs;
  j += F(",\"left\":");     j += left;
  j += F(",\"cl\":");       j += WiFi.softAPgetStationNum();
  j += F(",\"lvl\":");      j += lvl;
  j += F(",\"cat\":");      j += cat;
  j += F(",\"item\":");     j += item;
  j += F(",\"bright\":");   j += oledBrightness;
  j += F(",\"ap\":");       j += (int)(webuiIsEnabled() ? 1 : 0);
  j += F(",\"tok\":\"");    j += gToken;
  j += F("\"}");

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

static void hMenu() {
  String j;
  j.reserve(768);
  j += F("{\"cats\":[");
  for (int c = 0; c < webMenuCatCount(); c++) {
    if (c) j += ',';
    j += F("{\"n\":\""); j += webMenuCatName(c); j += F("\",\"i\":[");
    for (int i = 0; i < webMenuItemCount(c); i++) {
      if (i) j += ',';
      j += '"'; j += webMenuItemLabel(c, i); j += '"';
    }
    j += F("]}");
  }
  j += F("]}");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

static void hFrame() {
  static uint8_t buf[1024];
  portENTER_CRITICAL(&gFrameMux);
  memcpy(buf, gFrame, sizeof(buf));
  portEXIT_CRITICAL(&gFrameMux);
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(sizeof(buf));
  server.send(200, "application/octet-stream", "");
  server.sendContent(reinterpret_cast<const char*>(buf), sizeof(buf));
}

static void hInput() {
  if (!authed()) { deny(); return; }
  String b = server.arg("b");
  if      (b == "left")   ChangeRequested  = true;
  else if (b == "right")  ChangeRequested1 = true;
  else if (b == "select") SelectRequested  = true;
  else if (b == "back")   SelectBack       = true;
  else { server.send(400, "text/plain", "bad key"); return; }
  server.send(200, "text/plain", "ok");
}

static void hExec() {
  if (!authed()) { deny(); return; }
  if (gToolActive) { server.send(409, "text/plain", "busy"); return; }

  int c = server.arg("c").toInt();
  int i = server.arg("i").toInt();
  if (c < 0 || c >= webMenuCatCount() || i < 0 || i >= webMenuItemCount(c)) {
    server.send(400, "text/plain", "bad target");
    return;
  }
  portENTER_CRITICAL(&gExecMux);
  gPendCat  = c;
  gPendItem = i;
  portEXIT_CRITICAL(&gExecMux);
  server.send(200, "text/plain", "queued");
}

static void hStop() {
  if (!authed()) { deny(); return; }
  SelectBack = true;           // every tool loop and the menu honour this
  server.send(200, "text/plain", "stopped");
}

static void hConfig() {
  if (!authed()) { deny(); return; }
  if (server.hasArg("autostop")) {
    long v = server.arg("autostop").toInt();
    if (v <= 0) v = 0;                       // explicit OFF
    else if (v < 5000)    v = 5000;          // never below 5 s
    else if (v > 600000)  v = 600000;        // never above 10 min
    gAutostopMs = (uint32_t)v;
  }
  if (server.hasArg("bright")) {
    long b = server.arg("bright").toInt();
    if (b >= 5 && b <= 255) webSetBrightness((uint8_t)b);
  }
  server.send(200, "text/plain", "ok");
}

static void hNotFound() { server.send(404, "text/plain", "not found"); }

// --------------------------------------------------------------- lifecycle ---

void webuiSnapshot() {
  const uint8_t* src = u8g2.getBufferPtr();
  if (!src) return;
  portENTER_CRITICAL(&gFrameMux);
  memcpy(gFrame, src, sizeof(gFrame));
  portEXIT_CRITICAL(&gFrameMux);
}

void webuiToolEnter(const char* label) {
  strncpy(gToolLabel, label ? label : "", sizeof(gToolLabel) - 1);
  gToolLabel[sizeof(gToolLabel) - 1] = 0;
  gFailsafeFired = false;
  gToolStart  = millis();
  gToolActive = true;
}

void webuiToolExit() {
  gToolActive = false;
  gToolLabel[0] = 0;
  gFailsafeFired = false;
  if (!gEnabled) return;
  gRestoreAp = true;        // the web task owns the radio and the listening socket
}

bool webuiTakeExec(int& cat, int& item) {
  bool got = false;
  portENTER_CRITICAL(&gExecMux);
  if (gPendCat >= 0) {
    cat = gPendCat;
    item = gPendItem;
    gPendCat = -1;
    got = true;
  }
  portEXIT_CRITICAL(&gExecMux);
  return got;
}

static bool loadEnabled() {
  Preferences prefs;
  prefs.begin("rfclown", true);
  bool v = prefs.getBool("webui", true);
  prefs.end();
  return v;
}

void webuiPersistEnabled(bool on) {
  Preferences prefs;
  prefs.begin("rfclown", false);
  if (prefs.getBool("webui", true) != on) prefs.putBool("webui", on);   // flash-friendly
  prefs.end();
}

bool webuiIsEnabled() { return gEnabled; }

// Runs on the web task: the AP and the listening socket belong to it.
static void applyEnable(bool on) {
  if (on == gEnabled) return;
  if (on) {
    bringApUp();
    server.begin();
    gEnabled = true;
  } else {
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    gEnabled = false;
  }
}

void webuiSetEnabled(bool on) {
  gDesired = on;
  unsigned long t0 = millis();
  while (gEnabled != on && millis() - t0 < 1500) delay(5);
}

// Deadman switch: runs on the web task, so it fires even while the main task
// is stuck inside a tool loop. SelectBack unwinds any tool within one pass.
static void failsafeTick() {
  if (!gToolActive || gAutostopMs == 0) return;
  if (millis() - gToolStart >= gAutostopMs && !gFailsafeFired) {
    gFailsafeFired = true;
    SelectBack = true;
  }
}

static void webTask(void*) {
  for (;;) {
    if (gRestoreAp) {
      gRestoreAp = false;
      // WiFi tools can leave the interface in STA/APSTA or switched off entirely.
      wifi_mode_t mode = WiFi.getMode();
      if (mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA) {
        bringApUp();
        server.stop();
        server.begin();
      } else {
        esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
      }
    }
    if (gRestartServer) {
      gRestartServer = false;
      server.stop();
      server.begin();
    }
    if (gDesired != gEnabled) applyEnable(gDesired);
    if (gEnabled) server.handleClient();
    failsafeTick();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void webuiBegin() {
  if (gTaskStarted) return;

  snprintf(gToken, sizeof(gToken), "%08x", (unsigned)esp_random());

  server.on("/",                 HTTP_GET,  hRoot);
  server.on("/api/status",       HTTP_GET,  hStatus);
  server.on("/api/menu",         HTTP_GET,  hMenu);
  server.on("/api/frame",        HTTP_GET,  hFrame);
  server.on("/api/input",        HTTP_POST, hInput);
  server.on("/api/exec",         HTTP_POST, hExec);
  server.on("/api/stop",         HTTP_POST, hStop);
  server.on("/api/config",       HTTP_POST, hConfig);
  server.onNotFound(hNotFound);

  gDesired = loadEnabled();
  applyEnable(gDesired);          // brings the AP up only when enabled

  xTaskCreatePinnedToCore(webTask, "webui", 8192, nullptr, 1, nullptr, 0);
  gTaskStarted = true;
}
