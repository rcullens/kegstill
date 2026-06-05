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

## Libraries required

- **NimBLE-Arduino** (v2.x)  — `h2zero/NimBLE-Arduino`
- **ArduinoJson** (v6.x)     — keep on v6, the code uses `StaticJsonDocument`/`createNestedObject`
- **Preferences**, **LittleFS** (built-in with ESP32 core 2.x+)
- **WebServer**, **WiFi**, **ArduinoOTA** (built-in)

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
