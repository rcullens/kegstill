// config.h - pins, constants, default WiFi
#pragma once
#include <Arduino.h>

// ====== HARDWARE ======
#define SSR_PIN              5     // GPIO5 -> Relay board -> Omron G3NA-240B-UTU
#define LED_PIN              2     // onboard LED (optional)

// Motorized ball valve: 3-wire "smart" 9-24 VDC actuator with internal limit
// switches (the valve cuts its own motor at end of travel). ESP32 just drives
// two signals through relays/MOSFETs that switch the OPEN and CLOSE lines.
//   VALVE_OPEN_PIN          drive OPEN line  (HIGH = energize OPEN winding)
//   VALVE_CLOSE_PIN         drive CLOSE line (HIGH = energize CLOSE winding)
// Never assert both — the firmware enforces a reverse dwell when changing
// direction. Position is estimated purely by timing against calibrated full-
// travel durations (no external limit-switch feedback wires).
#define VALVE_OPEN_PIN          18
#define VALVE_CLOSE_PIN         19

// On boot we re-home by driving CLOSE for closeTimeMs * this multiplier; the
// extra time is harmless because the valve's internal limit switch cuts the
// motor when it reaches the closed stop. 1.2 = 20% safety margin.
#define VALVE_HOMING_OVERSHOOT  1.2f

// Direction-reversal dwell — both outputs LOW for this long when reversing.
#define VALVE_REVERSE_DWELL_MS  150UL

// Deadband — stop within this many % of target.
#define VALVE_DEADBAND_PCT      1.0f

// ====== TIMING ======
const unsigned long CONTROL_PERIOD_MS  = 250;   // PID/stage update
const unsigned long STATUS_PERIOD_MS   = 650;   // session sampling
const unsigned long PWM_WINDOW_MS      = 5000;  // SSR slow-PWM window (zero-cross relay)
const unsigned long BLE_TIMEOUT_MS     = 30000; // ESTOP after this without a BLE reading
const unsigned long PERSIST_RUN_MS     = 30000; // save run metadata every 30s
const unsigned long MAX_RUN_MS         = 24UL * 60UL * 60UL * 1000UL; // 24h hard ceiling

// ====== SAFETY ======
const float  HARD_MAX_TEMP_C   = 105.0;
const float  HARD_MIN_TEMP_C   = -10.0;
const size_t MAX_SESSION_PTS   = 500;

// ====== DEFAULT WIFI (fallback if NVS empty) ======
extern const char* WIFI_SSID_DEFAULT;
extern const char* WIFI_PASS_DEFAULT;

// ====== OTA ======
extern const char* OTA_HOSTNAME;
extern const char* OTA_PASSWORD;

// ====== HELPERS ======
inline float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
inline float fToC(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
