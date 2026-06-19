// config.h - pins, constants, default WiFi
#pragma once
#include <Arduino.h>

// ====== HARDWARE ======
#define SSR_PIN              5     // GPIO5 -> Relay board -> Omron G3NA-240B-UTU
#define LED_PIN              2     // onboard LED (optional)

// Motorized ball valve
#define VALVE_OPEN_PIN          18
#define VALVE_CLOSE_PIN         19

// MAX31856 K-type thermocouple amplifier (software SPI).
// UPDATED TO MATCH YOUR HARDWARE LOGS (17, 21, 22, 23)
#define TC_CS_PIN               17
#define TC_SDI_PIN              21   // MOSI: ESP32 -> MAX31856
#define TC_SDO_PIN              22   // MISO: MAX31856 -> ESP32
#define TC_SCK_PIN              23

#define VALVE_HOMING_OVERSHOOT  1.2f
#define VALVE_REVERSE_DWELL_MS  150UL
#define VALVE_DEADBAND_PCT      1.0f

// ====== TIMING ======
const unsigned long CONTROL_PERIOD_MS  = 250;   // PID/stage update
const unsigned long STATUS_PERIOD_MS   = 650;   // session sampling
const unsigned long PWM_WINDOW_MS      = 5000;  // SSR slow-PWM window
const unsigned long BLE_TIMEOUT_MS     = 30000; // ESTOP after this without a probe reading
const unsigned long PERSIST_RUN_MS     = 30000; // save run metadata every 30s
const unsigned long MAX_RUN_MS         = 24UL * 60UL * 60UL * 1000UL;

// ====== SAFETY ======
const float  HARD_MAX_TEMP_C   = 105.0;
const float  HARD_MIN_TEMP_C   = -10.0;
const size_t MAX_SESSION_PTS   = 500;

// ====== DEFAULT WIFI ======
extern const char* WIFI_SSID_DEFAULT;
extern const char* WIFI_PASS_DEFAULT;

// ====== OTA ======
extern const char* OTA_HOSTNAME;
extern const char* OTA_PASSWORD;

// ====== HELPERS ======
inline float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
inline float fToC(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
