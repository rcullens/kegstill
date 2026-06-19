// config.h - pins, constants, default WiFi
#pragma once
#include <Arduino.h>

// ====== HARDWARE ======
// GOD MODE PATCH: MOVING SSR OFF GPIO 5 TO AVOID 1.8V FLASH BROWNOUT STRAP
#define SSR_PIN              32    // MOVED FROM 5 TO 32
#define LED_PIN              2     

#define VALVE_OPEN_PIN          18
#define VALVE_CLOSE_PIN         19

// Thermocouple pins confirmed from your logs (17, 21, 22, 23)
#define TC_CS_PIN               17
#define TC_SDI_PIN              21   
#define TC_SDO_PIN              22   
#define TC_SCK_PIN              23

#define VALVE_HOMING_OVERSHOOT  1.2f
#define VALVE_REVERSE_DWELL_MS  150UL
#define VALVE_DEADBAND_PCT      1.0f

// ====== TIMING ======
const unsigned long CONTROL_PERIOD_MS  = 250;   
const unsigned long STATUS_PERIOD_MS   = 650;   
const unsigned long PWM_WINDOW_MS      = 5000;  
const unsigned long BLE_TIMEOUT_MS     = 30000; 
const unsigned long PERSIST_RUN_MS     = 30000; 
const unsigned long MAX_RUN_MS         = 24UL * 60UL * 60UL * 1000UL;

// ====== SAFETY ======
const float  HARD_MAX_TEMP_C   = 105.0;
const float  HARD_MIN_TEMP_C   = -10.0;
const size_t MAX_SESSION_PTS   = 500;

extern const char* WIFI_SSID_DEFAULT;
extern const char* WIFI_PASS_DEFAULT;
extern const char* OTA_HOSTNAME;
extern const char* OTA_PASSWORD;

inline float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
inline float fToC(float f) { return (f - 32.0f) * 5.0f / 9.0f; }
