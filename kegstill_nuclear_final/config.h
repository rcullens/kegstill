// config.h - pins, constants, default WiFi
#pragma once
#include <Arduino.h>

// ====== HARDWARE ======
#define SSR_PIN              5     // GPIO5 -> Relay board -> Omron G3NA-240B-UTU
#define LED_PIN              2     // onboard LED (optional)

// Motorized ball valve (PLACEHOLDER pins — set when you wire the actuator).
// Typical options:
//   (a) 2-wire DC motor + driver + limit switches -> drive via 2 GPIOs for OPEN/CLOSE
//   (b) servo / linear actuator -> single PWM GPIO
//   (c) stepper -> step/dir pair
// For now we only expose a 0-100% setpoint. Update valve.cpp once you pick.
#define VALVE_OPEN_PIN       18
#define VALVE_CLOSE_PIN      19
#define VALVE_FEEDBACK_PIN   34    // optional ADC for position feedback

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
