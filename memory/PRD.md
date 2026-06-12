# Keg Still - Glitch Edition (ESP32 firmware)

## Problem
Build an ESP32-based distillation controller that drives a 120VAC heater via SSR (PWM), measures vapor temperature with a K-type thermocouple (MAX31856), drives a 3-wire DC motorized ball valve, and serves a web dashboard. User has been hitting brownouts at WiFi init and demanded "make it work."

## Hardware
- ESP32-WROOM-DA dev board
- Adafruit MAX31856 K-type amplifier (software SPI: CS=15, SDI=13, SDO=27, SCK=14)
- Omron G3NA SSR on pin 5 (heater)
- 3-wire DC motorized ball valve on pins 18 (OPEN) / 19 (CLOSE)

## Files (single source of truth)
- `/app/kegstill_mvp/kegstill_mvp.ino` - all firmware
- `/app/kegstill_mvp/index_html.h` - PROGMEM web dashboard (HTML/Tailwind CDN/Chart.js/vanilla JS)

`/app/kegstill_nuclear_final/` is abandoned multi-file code, ignore.

## Implemented (Feb 2026)
- Bulletproof boot: brownout detector off, WiFi TX dropped to 8.5 dBm, no sleep, 30s connect timeout with restart fallback.
- Manual heater PWM (5s window) with hard E-STOP > 105 C or probe loss.
- 3-wire valve driver with proportional positioning, calibration (open/close ms), home-on-boot.
- 4-stage automation: WARMUP -> HEADS -> HEARTS -> TAILS -> DONE. Each stage has temp trigger, power %, valve %, max-minutes. Hard shutoff at user-set temp.
- Profile system in NVS (`Preferences.h`) - persists across reboots.
- In-RAM run history (720 samples @ 10s = 2h), JSON + CSV export, clear endpoint.
- Dilution calculator (Pearson's square, client-side).
- Pre-flight confirmation modal before start.
- Tabbed dashboard: RUN / PROFILE / HISTORY / DILUTION / VALVE.

## API
- `GET /api/status`, `GET /api/probe`, `GET /api/valve/status`
- `POST /api/power` (manual only; rejected during auto)
- `POST /api/valve` (pos | cmd | calibration)
- `POST /api/start`, `/api/stop`, `/api/estop`, `/api/reset`
- `GET/POST /api/profile`
- `POST /api/auto/start`, `/api/auto/stop`
- `GET /api/history`, `GET /api/history.csv`, `DELETE /api/history`

## Pending / Backlog (P1)
- Telegram/Discord push notifications on stage transitions and E-STOP.
- Multiple named profiles in NVS (currently single profile slot).
- WiFi credential captive portal (currently hardcoded).
- OTA firmware update.

## Known constraints
- Cannot be tested on the agent platform — Arduino C++ for ESP32 hardware. User must flash and report serial monitor output.
- WiFi creds are hardcoded: `Ponderosa` / `Biggs490$!`.
