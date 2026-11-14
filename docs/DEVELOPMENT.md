# RF Clown X — Developer & Build Guide

Developer-facing notes for building and extending the firmware in
[`RFClownX/`](../RFClownX). For the user-facing overview and the fork's feature list,
see the [README](../README.md).

---

## Hardware

- **MCU:** ESP32-WROOM-32U (4 MB flash)
- **Radios:** 3× GT24 Mini (nRF24L01+ compatible) on the shared SPI bus
- **Display:** 0.96" SSD1306 128×64 OLED (I²C)
- **Input:** 3 tactile buttons (left / right / select)
- **Indicator:** 1× WS2812 NeoPixel
- **Programming / power:** CP2102 USB-UART, TP4056 charger, LF33 3.3 V regulator

### Pinout

| Function | GPIO |
|----------|------|
| Button — Left | 27 |
| Button — Right | 25 |
| Button — Select | 26 |
| NeoPixel | 14 |
| nRF24 A — CE / CSN | 5 / 17 |
| nRF24 B — CE / CSN | 16 / 4 |
| nRF24 C — CE / CSN | 15 / 2 |
| OLED | I²C defaults — SDA 21 / SCL 22 |

> GPIO 4 is Radio B's CSN, which is why the NeoPixel is defined on GPIO 14
> (`PIN_NEOPIXEL` in [`config.h`](../RFClownX/config.h)).

---

## Toolchain

- **ESP32 Arduino core:** `esp32:esp32` 3.3.x. The upstream project targeted
  1.0.x; this fork builds on the modern core.
- **Libraries:** `RF24`, `U8g2`, `Adafruit NeoPixel`. BLE, WiFi, Preferences and
  the rest come with the core.

The exact versions below are the ones this firmware has been compiled with:

| Component | Version |
|-----------|---------|
| ESP32 core | 3.3.12 |
| RF24 | 1.6.2 |
| U8g2 | 2.36.19 |
| Adafruit NeoPixel | 1.15.5 |

That build used 1,733,816 bytes of program space (55% of the 3 MB partition) and
72,816 bytes of static RAM (22%). Library versions drift — if a newer release
fails to compile, pin these before assuming the code is wrong.

### Install (arduino-cli)

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "RF24" "U8g2" "Adafruit NeoPixel"
```

---

## Build & flash

> **Important:** the firmware links the full WiFi **and** BLE stacks (~1.7 MB),
> which does not fit the default 1.3 MB app partition. Use **Huge APP (3 MB)**.

```bash
# Compile and upload (replace the port with your own)
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" -u -p /dev/ttyUSB0 RFClownX

# Compile only, exporting the binaries to RFClownX/build/
arduino-cli compile --fqbn "esp32:esp32:esp32:PartitionScheme=huge_app" --export-binaries RFClownX
```

In the **Arduino IDE**: select *ESP32 Dev Module*, then
**Tools → Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)**.

Do not commit the generated `RFClownX/build/` folder.

---

## Firmware architecture

### Entry points

`setup()` initialises I²C and the OLED, loads persisted settings, shuts down the
BT and WiFi stacks, starts the NeoPixel, attaches the button interrupts,
configures the three radios and parks them powered down. `loop()` does nothing
but run the menu.

Every tool is **blocking**: `loop()` calls the tool function and does not return
until the tool exits. That is why tools poll the button flags themselves.

### Menu model

The menu is a two-level table, not a list of mode enums:

```cpp
struct MenuItem { const char* label; ToolFn fn; const byte* ch; uint8_t n; };
struct MenuCat  { const char* name; const MenuItem* items; uint8_t count; };
```

- `gCats[]` holds five categories: Jammers, Scanners, Attacks, Games, System.
- `gLevel` is 0 for the category list and 1 for the item list; `gCatSel` and
  `gItemSel` are the cursors. `drawMenu()` shows four rows and pages by four as
  the cursor moves.
- An item with `fn != nullptr` is a **tool** — Select calls `fn()`.
- An item with `fn == nullptr` and a channel list is a **jammer** — Select calls
  `openJammer(label, ch, n)`.

**To add a menu entry:** append to the matching `…Items[]` array and update the
count in `gCats[]`. Those counts are written by hand (`8, 4, 6, 1, 5`), so a
stale count silently hides the last entry. `sizeof(array) / sizeof(array[0])` is
the safer form whenever you touch this table.

### Buttons

Buttons are interrupt-driven and only set flags; all decisions happen in normal
code.

| Button | ISR | Flag | Menu action |
|--------|-----|------|-------------|
| Left | `handleButton` | `ChangeRequested` | previous entry |
| Right | `handleButton1` | `ChangeRequested1` | next entry |
| Select | `handleButton2` | `SelectRequested` / `SelectBack` | select / back |

- Left and Right have **independent** debounce timestamps (`lastPressL`,
  `lastPressR`, 200 ms). They used to share one, which dropped a fast
  left-then-right press.
- Select is edge-detected on `CHANGE`: a press records `selDownTime`, a release
  under 40 ms is discarded as contact bounce, a release under `longPressMs` sets
  `SelectRequested`, and a longer hold sets `SelectBack`.
- `longPressMs` is loaded from NVS (key `holdms`, clamped to 200–1500 ms) and is
  edited in *System → Back Time*.

### Tool contract

Every tool follows the same pattern:

1. Clear `ChangeRequested`, `ChangeRequested1`, `SelectRequested` and `SelectBack`
   on entry, so a stale edge does not fire immediately.
2. Loop until `SelectBack`, polling the flags it cares about and clearing each
   one after handling it.
3. Call `toolLEDTick()` from the loop if the tool blocks for long — it blinks the
   NeoPixel green about every 200 ms.
4. On exit, clear `SelectRequested` and `SelectBack`, power the radios back down
   if it used them, and call `update_OLED()` so the menu is redrawn.

`loop()` paints the NeoPixel green before calling a tool and blue afterwards, so
tools do not manage the idle colour themselves.

### Jammers

`openJammer(name, chans, n)` puts all three radios into constant-carrier mode and
keeps them transmitting until the user goes back:

- `sel == 0` is **sweep**; `sel == 1..n` selects a single list entry.
- In single-channel mode the radios sit on `c-1`, `c` and `c+1`, so the marked
  channel is roughly 3 MHz wide; the label shows the nominal channel.
- Sweep is either **random** (re-rolled continuously) or **sequential** (three
  consecutive list entries, 2 s per step, advancing by three). Short Select on
  the sweep entry toggles between them.
- Exit powers the radios down and calls `setRadiosNeutralState()`.

### Channel maps

NRF24 transmits at `2400 + channel` MHz, channels `0..125`. The lists live in
[`config.h`](../RFClownX/config.h):

| Set | NRF24 channels |
|-----|----------------|
| WiFi | 1–84 (every 1 MHz slot) |
| Video TX | 70, 75, 80 |
| RC | 1, 3, 5, 7 |
| BLE | 2, 26, 80 |
| Bluetooth | 21 channels — see `bluetooth_channels[]` |
| USB Wireless | 40, 50, 60 |
| Zigbee | 11, 15, 20, 25 |
| NRF24 | 76, 78, 79 |

WiFi reference points: WiFi ch1 = 2412 MHz = NRF24 ch 12, ch6 = 2437 = 37,
ch11 = 2462 = 62, ch14 = 2484 = 84.

### Scanners

`openWiFiScan()` switches to `WIFI_STA`, runs a blocking `scanNetworks()`, copies
up to 20 results into function-static arrays, frees the scan and turns WiFi off.
`openBLEScan()` does the same with `BLEScan::start(4, false)` — on the 3.x core
that returns `BLEScanResults*`, not a value. Both open a detail screen on short
Select and return to the menu on hold.

### Display and persistence

- Brightness writes the SSD1306 contrast (`0x81`) together with pre-charge
  (`0xD9`) and VCOMH (`0xDB`), because contrast alone is nearly invisible on
  these panels. NVS namespace `rfclown`, key `bright`.
- Persisted values are only written when they actually change, to spare flash.

### Globals

Header-level globals are `inline` on purpose. The modern core builds with GCC's
`-fno-common`, so plain definitions in a header included by several translation
units would produce *multiple definition* link errors.

---

### Remote WebUI

Optional service in `webui.h` / `webui.cpp` / `webui_page.h`, started at the end of
`setup()`. The HTML is a single PROGMEM blob (`webui_page.h`, ~14 KB) with no external
assets, so it works with the device offline.

- **Task model.** `webuiBegin()` brings up the SoftAP and starts `webTask` pinned to core 0
  (`xTaskCreatePinnedToCore`, 8 KB stack). The RF tools keep the Arduino loop busy on
  core 1, so the server stays responsive without refactoring the tool loops.
- **Enable/disable.** `System -> Remote AP` calls `webuiSetEnabled()`, which posts a
  start/stop request to the web task (only that task touches the AP and the socket).
  The state is persisted in NVS under the `webui` key and defaults to on at first boot.
- **No shared hardware.** The web task never calls `u8g2`, the radios or the NeoPixel:
  - the main task copies `u8g2.getBufferPtr()` into `gFrame` under a spinlock
    (`webuiSnapshot()`) and `/api/frame` serves that 1 KB page-major bitmap. That backs the
    page's `Pixels` view; the default `Crisp` view re-draws the same layout from
    `/api/menu` and the `lvl`/`cat`/`item`/`tool`/`ap` fields of `/api/status`, which is
    what keeps the screen readable at phone size;
  - launches use a one-slot queue (`/api/exec` writes `gPendCat`/`gPendItem`, `loop()`
    consumes it and runs the tool on the main task through `webMenuExec()`);
  - virtual buttons set the same `ChangeRequested` / `SelectRequested` / `SelectBack`
    flags the ISRs use, so both input paths share one code path.
- **Failsafe.** `webuiToolEnter()` stamps `gToolStart`; `failsafeTick()` runs on the web
  task and raises `SelectBack` when the deadline passes. Every tool loop tests that flag,
  so the tool unwinds within one iteration even if the HTTP link is dead.
- **Menu access.** `gCats[]` is file-static in the sketch, so `webMenuCatCount()`,
  `webMenuCatName()`, `webMenuItemCount()` and `webMenuItemLabel()` expose read-only
  views to the service.
- **Routes.** `GET /`, `/api/status`, `/api/menu`, `/api/frame`;
  `POST /api/input?b=left|right|select|back`, `/api/exec?c=<cat>&i=<item>`, `/api/stop`,
  `/api/config?autostop=<ms>&bright=<5-255>`.
- **Auth.** POSTs require an `X-Token` header, minted from `esp_random()` per boot and
  handed out by `/api/status`. That is a CSRF guard, not a replacement for the WPA2 key.
- **Wi-Fi conflict.** Tools that call `WiFi.mode(WIFI_OFF)` or `esp_wifi_set_channel()`
  take the AP with them. `webuiToolExit()` restarts the AP, re-locks channel 1 and asks
  the web task to rebind the listening socket.


## Code layout

| File | Contents |
|------|----------|
| `RFClownX.ino` | ISRs, menu, every tool, `setup()` / `loop()` |
| `config.h` | pins, hardware objects, channel maps, button flags |
| `setting.h` | shared declarations and the 128×64 splash bitmap |
| `setting.cpp` | nRF24 configuration and the splash sequence |
| `neopixel.cpp` | NeoPixel colour helper |

---

## Known limitations

- Menu counts in `gCats[]` are hardcoded; see above.
- `esp_wifi_set_channel()` return values are ignored in the WiFi spam tools, so
  a failed channel switch is not reported.
- RC Replay captures frames but transmits them to a fixed 2-byte address instead
  of the captured address, so it does not reproduce captured traffic.
- The MouseJack promiscuous parser assumes a particular bit alignment. If its
  packet counter never advances, check the CRC and bit offset before suspecting
  the hardware.
- `BLEDevice::deinit(false)` keeps the BT controller memory allocated, and the
  BadBLE server/HID objects are allocated on every entry.
- `u8g2.setBitmapMode(1)` is set during the splash and never reset.
- `cred[]` in `setting.h` is an unused 1 KB bitmap.
- There is no prebuilt binary for the current firmware version; see the README.
