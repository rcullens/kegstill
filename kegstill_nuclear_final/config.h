// config.h - pins, constants, default WiFi
#pragma once
#include <Arduino.h>

// ====== HARDWARE ======
#define SSR_PIN              5     // GPIO5 -> Relay board -> Omron G3NA-240B-UTU
#define LED_PIN              2     // onboard LED (optional)

// Motorized ball valve: 2-wire DC actuator with end limit switches.
//   VALVE_OPEN_PIN          drive OPEN direction (HIGH = motor running open)
//   VALVE_CLOSE_PIN         drive CLOSE direction (HIGH = motor running close)
//   VALVE_LIMIT_OPEN_PIN    input, active LOW (pull-up) when fully open
//   VALVE_LIMIT_CLOSED_PIN  input, active LOW (pull-up) when fully closed
// Wire the limit switches to GND through the switch with INPUT_PULLUP on the ESP32.
// Position 0% = fully closed, 100% = fully open. Intermediate positions are
// estimated by timing against the calibrated full-travel duration.
#define VALVE_OPEN_PIN          18
#define VALVE_CLOSE_PIN         19
#define VALVE_LIMIT_OPEN_PIN    34   // input-only pins 34-39 OK; use pull-up
#define VALVE_LIMIT_CLOSED_PIN  35

// Safety timing — if motor runs this long without hitting the expected limit,
// declare FAULT. Set to ~1.5x the slowest expected full-travel for your valve.
#define VALVE_MAX_TRAVEL_MS     45000UL

// Direction-reversal dwell — both outputs LOW for this long when reversing.
// Prevents H-bridge shoot-through and reduces motor stress.
#define VALVE_REVERSE_DWELL_MS  100UL

// Deadband — stop within this many % of target. Inertia after stop usually
// adds another fraction of a percent.
#define VALVE_DEADBAND_PCT      0.5f

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
