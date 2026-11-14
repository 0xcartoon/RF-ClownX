/* ____________________________
   Remote WebUI for RF Clown X (optional service)
   MIT License - see LICENSE / README for the base project (CiferTech RF-Clown)
   ________________________________________ */

#ifndef WEBUI_H
#define WEBUI_H

#include <Arduino.h>

// ---- Bridge implemented in RFClownX.ino (menu lives there) ----
int         webMenuCatCount();
const char* webMenuCatName(int cat);
int         webMenuItemCount(int cat);
const char* webMenuItemLabel(int cat, int item);
void        webMenuState(int& level, int& cat, int& item);
void        webMenuExec(int cat, int item);      // runs a tool on the MAIN task
void        webSetBrightness(uint8_t value);

// ---- Service implemented in webui.cpp ----
void webuiBegin();                    // start SoftAP + server task (call from setup)
void webuiSnapshot();                 // main task: publish the OLED frame
void webuiToolEnter(const char* label);  // wrap a tool invocation
void webuiToolExit();
bool webuiTakeExec(int& cat, int& item); // main task: consume a queued launch

// Runtime on/off (System -> Remote AP), persisted in NVS
bool webuiIsEnabled();
void webuiSetEnabled(bool on);       // requests the web task to start/stop the AP
void webuiPersistEnabled(bool on);

#endif // WEBUI_H
