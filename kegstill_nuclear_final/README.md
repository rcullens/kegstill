# KEG STILL — GLITCH EDITION (split build)

ESP32 firmware for an automated 4-stage still controller, driving an Omron G3NA-240B SSR
via GPIO5, reading vapor temperature from a ThermoPro CQ60 BLE thermometer, and serving
a web UI on the LAN.

## Files

```
kegstill_nuclear_final/
├── kegstill_nuclear_final.ino   main: globals, setup(), loop()
├── config.h                     pins, timing, safety limits, defaults
├── state.h                      data structures + extern globals
├── storage.h / storage.cpp      NVS persistence (Preferences)
├── ble_scanner.h / .cpp         CQ60 manufacturer-data parser, NimBLE scan
├── control.h / .cpp             4-stage state machine + slow-PWM SSR
├── valve.h / .cpp               motorized ball valve (PLACEHOLDER driver)
├── history.h / .cpp             per-run history on LittleFS
├── web_handlers.h / .cpp        HTTP routes
└── index_html.h                 web UI as PROGMEM raw string
```

Open the folder in Arduino IDE (file/folder name must match), select the **ESP32 Dev Module**
board (WROOM-DA works), and Upload.

## Install

### 1. Install Arduino IDE 2.x
Download from <https://www.arduino.cc/en/software> and install.

### 2. Add the ESP32 board package
1. **File → Preferences → Additional Boards Manager URLs**, paste:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager…**, search **esp32**, install **"esp32" by Espressif Systems** (version **2.0.14 or newer** — required for LittleFS and NimBLE v2 compatibility).

### 3. Install the two non-built-in libraries
**Sketch → Include Library → Manage Libraries…**, install:

| Library | Version | Why |
|---|---|---|
| **NimBLE-Arduino** (by h2zero) | **2.x** | BLE scan for CQ60. Code uses v2 API (`NimBLEScanCallbacks`, `const NimBLEAdvertisedDevice*`). v1.x will NOT compile. |
| **ArduinoJson** (by Benoît Blanchon) | **6.21.x** (any 6.x) | JSON parse/build. Code uses v6 syntax (`StaticJsonDocument`, `createNestedObject`). Do **not** install v7. |

Built-in (ship with the ESP32 core, no install needed): `WiFi`, `WebServer`, `ArduinoOTA`, `Preferences`, `LittleFS`, `NimBLEDevice` is the one above.

### 4. Get the sketch into your Arduino sketchbook
Either:
- Copy the entire `kegstill_nuclear_final/` folder into your Arduino sketchbook (typically `~/Documents/Arduino/` on Mac, `Documents\Arduino\` on Windows), **or**
- Open it in place: **File → Open…** → pick `kegstill_nuclear_final.ino`. All `.h`/`.cpp` siblings auto-load as tabs.

> The **folder name must match the `.ino` filename** (`kegstill_nuclear_final`). Don't rename one without the other.

### 5. Pick board + partition scheme + port
- **Tools → Board → esp32 → ESP32 Dev Module**
- **Tools → Flash Size → 4MB (32Mb)**
- **Tools → Partition Scheme → "Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)"**
  This is **required** — the default 1.2 MB app partition is too small (the firmware is ~1.4 MB once NimBLE + WebServer + LittleFS + embedded HTML are linked in). Minimal SPIFFS gives ~1.9 MB for code (plenty of headroom) and ~190 KB for LittleFS run history (good for ~7-8 saved runs). For more history capacity at the cost of OTA, use "No OTA (2MB APP / 2MB SPIFFS)".
- **Tools → Upload Speed → 921600**
- **Tools → Core Debug Level → None** (or "Error" for less serial spam)
- **Tools → PSRAM → Disabled** (WROOM-DA has no PSRAM)
- **Tools → Port →** your USB port (Mac: `/dev/cu.usbserial-*`, Linux: `/dev/ttyUSB0`, Windows: `COMx`).

If the port doesn't appear, install the CP210x or CH340 USB-serial driver depending on which chip is on your board.

### 6. (Optional but recommended) Set OTA password and WiFi defaults
Open `kegstill_nuclear_final.ino` and edit:
```cpp
const char* WIFI_SSID_DEFAULT = "Ponderosa";
const char* WIFI_PASS_DEFAULT = "Biggs490$!";
const char* OTA_PASSWORD      = "kegstill";   // CHANGE THIS
```
The WiFi defaults are only used the first time; after that, anything saved via the UI's SYSTEM tab takes precedence (stored in NVS).

### 7. First flash
1. Plug ESP32 into USB.
2. Click **Upload** (right-arrow icon). First build pulls a lot of dependencies — give it 1–2 minutes.
3. Hold the **BOOT** button on the WROOM-DA if you see `Connecting……___` errors (some boards need it; many auto-reset fine).
4. Open **Serial Monitor** at **115200 baud**. You should see:
   ```
   [BOOT] Keg Still GLITCH (split build)
   [FS] LittleFS ok, 0 bytes used / 1572864 total
   [WiFi] connecting to 'Ponderosa'
   .......
   [WiFi] OK ip=192.168.1.xxx rssi=-52
   [BLE] Scan started
   [WEB] server up
   [READY] http://192.168.1.xxx
   ```
5. Open that IP in a browser on the same WiFi. You should see the GLITCH dashboard.

### 8. After the first flash — OTA updates
Once the device is on WiFi, subsequent flashes can be wireless:
- In Arduino IDE, **Tools → Port → Network ports → kegstill-cq60 at 192.168.1.xxx**
- Click Upload. Enter the OTA password when prompted.

### 9. First-boot data behavior
- LittleFS auto-formats on first mount (one-time, takes ~3 sec).
- NVS is empty → seeds the 3 default profiles → saves them.
- No run history yet → HISTORY tab shows "No saved runs yet."
- Power slider and valve slider both start at 0.
- Wait until the BLE pill turns **green** (CQ60 advertising detected) before clicking **DISTILL THIS SHIT!** — Start refuses without a valid probe by design.

### 10. Reset to factory
Two options:
- **Quick:** comment out the NVS load calls in `setup()`, re-flash. Then uncomment and re-flash.
- **Nuclear:** in Arduino IDE menu, **Tools → Erase All Flash Before Sketch Upload → Enabled**, upload once, then set it back to Disabled. Wipes NVS, LittleFS, the lot.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `Sketch too big; text section exceeds available space` | Wrong partition scheme. Set **Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**. |
| `'function' does not name a type` | You re-merged HTML into the `.ino`. Keep it in `index_html.h`. |
| `NimBLEScanCallbacks does not name a type` | NimBLE-Arduino is v1.x. Upgrade to v2.x. |
| `StaticJsonDocument is deprecated` warnings | You installed ArduinoJson v7. Downgrade to v6.21.x. |
| `LittleFS.h: No such file` | ESP32 core older than 2.0. Upgrade core to ≥2.0.14. |
| Compile fails with linker errors about multiple definitions | Two `.cpp` files in the sketchbook are defining the same global. Check you didn't duplicate `kegstill_nuclear_final/` somewhere else in the sketchbook. |
| Boot loop / `Guru Meditation Error` | Usually NVS corruption from a partition scheme change. Re-flash with "Erase All Flash" once. |
| Web UI loads but says "NO PROBE" forever | CQ60 not advertising, dead battery, or too far. Get the probe within ~5m of ESP32. Verify in Serial Monitor that BLE callbacks fire. |
| Start button does nothing | By design — Start refuses without valid BLE. Wait for green BLE pill. |
| Browser shows old UI | Hard-refresh (`Ctrl-Shift-R` / `Cmd-Shift-R`). Tailwind+Chart.js are cached from CDN. |

## Wiring summary

| Function | GPIO | Notes |
|---|---|---|
| SSR drive (heating element) | **5** | -> relay -> Omron G3NA-240B-UTU AC SSR |
| Valve **drive OPEN** | **18** | -> relay/MOSFET -> valve OPEN wire |
| Valve **drive CLOSE** | **19** | -> relay/MOSFET -> valve CLOSE wire |

**3-wire ball valve (9-24 VDC, internal limit switches):**
- COMMON wire -> + side of your 9-24 VDC supply (or AC live, if 110/220 V model)
- OPEN wire   -> driven by GPIO 18 via a relay/MOSFET
- CLOSE wire  -> driven by GPIO 19 via a relay/MOSFET
- DC supply ground -> back to your relays/MOSFETs

The valve's own internal limit switches cut the motor at end of travel, so it's safe to leave a drive signal asserted past the end. The firmware uses this with a 20% overshoot margin during boot-homing and any move to 0% or 100%.

Never assert both OPEN and CLOSE simultaneously — the firmware enforces a 150 ms reverse-dwell, but use SPDT relays or a properly-debounced driver IC to be safe.

## Valve calibration (manual)

Since this valve has no external position feedback, you calibrate it once with a stopwatch:

1. Open the dashboard. In the **BALL VALVE** panel, press **JOG CLOSE** and wait for the motor to stop (the internal limit cuts it).
2. Press **JOG OPEN** with a stopwatch running. Stop the watch the moment the motor stops. That's your **open time** in milliseconds (e.g. 8.3 s -> `8300`).
3. Press **JOG CLOSE** with the stopwatch. Stop when the motor stops. That's your **close time**.
4. Type both values into the Open time / Close time fields and press **SAVE CAL**.
5. Calibration is persisted to NVS and survives reboots. Re-calibrate any time the valve is serviced.

Once calibrated, intermediate positions (e.g. 35%) are estimated by running the motor for the proportional fraction of the calibrated travel time. Re-home occasionally with the **RE-HOME** button if drift creeps in — it just drives close for `closeTimeMs + 20%` to re-zero against the internal limit.

## CQ60 probe channel selection

The Chef iQ CQ60 advertises **6 separate temperature channels** in each BLE packet — the packet was reverse-engineered by [@willemcvu and @Ernst79](https://github.com/custom-components/ble_monitor/issues/1279). The 6 channels are:

| # | Channel | Source | Notes |
|---|---|---|---|
| 0 | Calc Ambient | algorithm | Caps at ~85 °C. The "ambient" reading on the black end of the probe. **NOT useful for still vapor.** |
| 1 | Calc Internal | algorithm | `min(tip, ring1, ring2)`. What the Chef iQ app shows as "the probe temperature". |
| 2 | **Tip** | raw sensor | **DEFAULT for still.** Sensor at the very tip of the metal probe — deepest in the vapor stream. |
| 3 | Ring 1 | raw sensor | Slightly back from the tip. |
| 4 | Ring 2 | raw sensor | Further back. |
| 5 | Ambient Raw | raw sensor | The thermocouple in the black plastic end. Caps at ~85 °C. **NOT useful for still vapor.** |

Choose via **BLE PROBE tab → Sensor Channel** dropdown. Selection persists across reboots.

If you found this firmware was reading wildly wrong temperatures before, that's why — the previous parser was grabbing Channel 0 (kitchen-air ambient) instead of the actual vapor reading. The new parser uses the byte-exact offsets above.

## Libraries summary (recap)

- **NimBLE-Arduino** v2.x — `h2zero/NimBLE-Arduino` (install via Library Manager)
- **ArduinoJson** v6.x — Benoît Blanchon (install via Library Manager; **not** v7)
- **WebServer**, **WiFi**, **ArduinoOTA**, **Preferences**, **LittleFS** — built into ESP32 core ≥2.0.14

## Why the split

Your original single-file build failed with:
```
error: 'function' does not name a type
```
That's the Arduino IDE preprocessor mangling the JS keyword `function` because it does not
respect C++11 raw string literals (`R"rawliteral(...)"`) inside `.ino` files — it scans
for prototypes and ignores raw-string boundaries. Moving the HTML into a plain `.h`
(`index_html.h`) bypasses that preprocessor.

## What changed vs. your original (bug audit)

| # | Issue | Fix |
|---|-------|-----|
| 1 | Compile error from `R"..."` in `.ino` | HTML moved to `index_html.h` |
| 2 | `targetPower += error*kp` integrates forever — sticks at 0% or maxPower | Replaced with true P-control: `cmd = headsPower + error*kp`, clamped |
| 3 | BLE-timeout estop guarded by `bleTempValid` so it never fires if probe never connected → could run with `currentTempC=25` default | `startRun()` now refuses to start without a valid probe; control loop ESTOPs if `!bleTempValid` |
| 4 | `cutTemp` defined but never enforced | Hard transition to `STAGE_SHUTDOWN` when `tempF >= cutTemp` |
| 5 | `sessionReadings.push_back` w/o `reserve()` fragments heap | `reserve(MAX_SESSION_PTS)` on start |
| 6 | Profile index OOB possible | Bounds-checked everywhere |
| 7 | No SSR off when stopped | Explicit `digitalWrite(SSR_PIN, LOW)` in `stopRun()` / `estop()` / when not running |
| 8 | No max-runtime guard | 24 h hard ceiling → ESTOP |
| 9 | OTA had no password | Set in `config.h` (change before deploying!) |

## New features

- **4-stage automation**: HEATUP → HEADS → HEARTS → TAILS → SHUTDOWN. Per-profile config
  for power/time/temp of each stage. P-control only runs in HEARTS.
- **NVS persistence (Preferences)**: profiles, current profile index, automation flag,
  batch info, WiFi creds, and a 30 s rolling snapshot of the active run.
- **Resume**: if the device loses power mid-run, on next boot the UI shows a modal asking
  "Resume Previous Run?" — pressing Resume continues the elapsed timer and stage from where
  it left off. Discard wipes the snapshot.
- **Stop / Skip-Stage** buttons.
- **WiFi config tab**: change SSID/pass from the UI; device reboots.
- **Delete profile**, profile editor with all stage parameters.
- **CSV export** now includes °C, °F, stage, and proper Content-Disposition.
- **Motorized ball valve (PLACEHOLDER)**: dashboard slider + auto-follow-stage toggle.
  Each profile carries per-stage valve setpoints (Heatup/Heads/Hearts/Tails). Hardware
  driver in `valve.cpp` is a stub — drop in your servo/stepper/DC-motor logic there.
  Default pins: `VALVE_OPEN_PIN=18`, `VALVE_CLOSE_PIN=19`, `VALVE_FEEDBACK_PIN=34`.
- **Per-run history on LittleFS**: every completed run is auto-saved to
  `/runs/run_NNNN.json`. New HISTORY tab lists all runs, plots a past run's temp/power/valve
  chart, downloads CSV, or deletes. Sequence number persists across reboots.
- **Dilution calculator**: new DILUTE tab. Inputs final jar volume (mL/L/oz/qt/gal),
  target ABV-or-proof, and source ABV (pulled from saved `washABV` or hand-entered
  hydrometer reading). Outputs distillate + water amounts in the same unit, plus a
  second unit for cross-check. Math is `V_target·ABV_target = V_source·ABV_source`
  (volume-additive approximation; TTB Table 6 if you need NIST-grade precision).

## Default profiles

| Name | Target °F | Max % | Kp | Cut °F | Heads (%, min) | Tails (°F, %) |
|------|-----------|-------|----|--------|----------------|---------------|
| Stripping Run        | 180 | 100 | 6.0 | 200 | 60% / 5 min  | 185 / 80% |
| Spirit Run - Hearts  | 172 |  65 | 4.5 | 195 | 25% / 15 min | 178 / 35% |
| Vodka / Neutral      | 172 |  55 | 3.5 | 188 | 20% / 20 min | 176 / 30% |

## Endpoints

```
GET  /                   web UI
GET  /api/status         live JSON
POST /api/start          {resume:bool}
POST /api/stop
POST /api/estop
POST /api/reset
POST /api/power          {power:0-100}
POST /api/automation     {enabled:bool}
POST /api/advance        skip to next stage
GET  /api/profiles
POST /api/profile/load   {index}
POST /api/profile/new    {name,...}
POST /api/profile/delete {index}
GET  /api/batch          batch info
POST /api/batch          set batch info
GET  /api/export         CSV of current session
POST /api/wifi           {ssid,pass}  reboots
POST /api/resume/dismiss
POST /api/valve          {pos:0-100, auto:bool}  set valve position or auto-follow
GET  /api/history        list of saved runs
GET  /api/history/get?id=NN     full run JSON
GET  /api/history/csv?id=NN     CSV download
POST /api/history/delete {id}
```

## Safety notes

This thing controls **120 VAC into a heating element under pressure**. Recommend:
- A mechanical thermal cutoff on the boiler (independent of firmware).
- A pressure relief valve.
- A second over-temperature sensor on the boiler wall, not just the vapor probe.
- Don't disable the BLE-timeout estop. If the CQ60 dies you want the elements off.
