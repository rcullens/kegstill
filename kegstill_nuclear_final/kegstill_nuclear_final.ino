/*
  KEG STILL - GLITCH EDITION - SPLIT BUILD
  ESP32 WROOM DA / Arduino IDE
  GPIO 5 -> Relay board -> Omron G3NA-240B-UTU
  BLE: ThermoPro CQ60 (manufacturer-data parse)
  WiFi creds and profiles persisted in NVS (Preferences).
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "state.h"
#include "storage.h"
#include "ble_scanner.h"
#include "control.h"
#include "web_handlers.h"
#include "valve.h"
#include "history.h"

// ========== constant definitions (config.h externs) ==========
const char* WIFI_SSID_DEFAULT = "Ponderosa";
const char* WIFI_PASS_DEFAULT = "Biggs490$!";
const char* OTA_HOSTNAME      = "kegstill-cq60";
const char* OTA_PASSWORD      = "kegstill";   // <-- change me

// ========== global state (state.h externs) ==========
std::vector<Profile>  profiles;
std::vector<Reading>  sessionReadings;
BatchInfo             currentBatch;

int   currentProfileIndex = 0;
float currentTempC        = 25.0f;
float currentPower        = 0.0f;
float targetPower         = 0.0f;
uint8_t currentValvePos   = 0;
bool  valveAutoFollowStage= true;
bool  automationEnabled   = false;
bool  isRunning           = false;
bool  estopActive         = false;
Stage currentStage        = STAGE_IDLE;

unsigned long startMillis        = 0;
unsigned long stageStartMillis   = 0;
unsigned long resumeOffsetMs     = 0;
unsigned long lastControlUpdate  = 0;
unsigned long lastStatusUpdate   = 0;
unsigned long lastPersistUpdate  = 0;
unsigned long windowStartTime    = 0;

bool          bleTempValid  = false;
unsigned long lastBleUpdate = 0;
int           bleBattery    = -1;

bool          resumePending     = false;
unsigned long resumeElapsedSec  = 0;
uint8_t       resumeStage       = STAGE_IDLE;
int           resumeProfileIdx  = 0;

WebServer server(80);

// ========== helpers ==========
String stageName(Stage s) {
  switch (s) {
    case STAGE_IDLE:     return "IDLE";
    case STAGE_HEATUP:   return "HEATUP";
    case STAGE_HEADS:    return "HEADS";
    case STAGE_HEARTS:   return "HEARTS";
    case STAGE_TAILS:    return "TAILS";
    case STAGE_SHUTDOWN: return "SHUTDOWN";
  }
  return "?";
}

// Rough vapor->ABV map (azeotrope shape). Inputs are °C.
float estimateABV(float tempC) {
  if (tempC < 78.0f)  return 96.0f;
  if (tempC > 99.5f)  return 8.0f;
  if (tempC < 80.0f)  return 95.0f - (tempC - 78.0f) * 2.5f;
  if (tempC < 85.0f)  return 90.0f - (tempC - 80.0f) * 4.0f;
  if (tempC < 90.0f)  return 70.0f - (tempC - 85.0f) * 4.0f;
  if (tempC < 95.0f)  return 50.0f - (tempC - 90.0f) * 6.0f;
  return 20.0f - (tempC - 95.0f) * 2.5f;
}

static void createDefaultProfiles() {
  profiles.clear();
  // Stripping: drive hard, wide cut window. Valve wide open through hearts.
  profiles.push_back({"Stripping Run", 180.0f, 100.0f, 6.0f, 200.0f,
                      100.0f, 60.0f, 300,  185.0f, 80.0f,
                      100, 100, 100, 100});
  // Hearts/spirit: gentler, tight cut. Valve throttled on heads/tails.
  profiles.push_back({"Spirit Run - Hearts", 172.0f, 65.0f, 4.5f, 195.0f,
                      100.0f, 25.0f, 900,  178.0f, 35.0f,
                      100, 35, 80, 50});
  // Vodka/neutral: lowest, tighter still.
  profiles.push_back({"Vodka / Neutral", 172.0f, 55.0f, 3.5f, 188.0f,
                      100.0f, 20.0f, 1200, 176.0f, 30.0f,
                      100, 25, 70, 40});
}

// ========== WiFi ==========
static bool connectWiFi() {
  String ssid, pass;
  if (!storage::loadWifi(ssid, pass) || ssid.length() == 0) {
    ssid = WIFI_SSID_DEFAULT;
    pass = WIFI_PASS_DEFAULT;
  }
  Serial.printf("[WiFi] connecting to '%s'\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid.c_str(), pass.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) { delay(400); Serial.print('.'); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[WiFi] FAILED"); return false; }
  Serial.printf("[WiFi] OK ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

// ========== setup / loop ==========
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[BOOT] Keg Still GLITCH (split build)");

  control::begin();
  valve::begin();
  history::begin();

  // Load persistent state first
  storage::begin();
  if (!storage::loadProfiles()) {
    Serial.println("[NVS] no profiles, seeding defaults");
    createDefaultProfiles();
    storage::saveProfiles();
  }
  currentProfileIndex = storage::loadCurrentProfileIndex();
  if (currentProfileIndex < 0 || currentProfileIndex >= (int)profiles.size()) currentProfileIndex = 0;

  automationEnabled = storage::loadAutomation();
  storage::loadBatch();
  storage::loadRunSnapshot();   // sets resumePending if applicable

  if (!connectWiFi()) {
    Serial.println("[WiFi] restarting in 5s"); delay(5000); ESP.restart();
  }
  delay(3000);  // LwIP settling on ESP32

  // OTA
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();

  // BLE
  ble_scanner::begin();
  ble_scanner::setSourceChannel(storage::loadBleSourceChannel());
  {
    String t = storage::loadBleTarget();
    if (t.length() > 0) ble_scanner::setTargetAddress(t);
  }

  // HTTP
  web_handlers::registerRoutes(server);
  server.begin();
  Serial.println("[WEB] server up");

  Serial.printf("[READY] http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  valve::poll();
  ble_scanner::poll();

  unsigned long now = millis();

  if (now - lastControlUpdate >= CONTROL_PERIOD_MS) {
    lastControlUpdate = now;
    control::update();
  }

  if (now - lastStatusUpdate >= STATUS_PERIOD_MS) {
    lastStatusUpdate = now;
    if (isRunning && !estopActive && sessionReadings.size() < MAX_SESSION_PTS) {
      Reading r;
      r.ts    = (now - startMillis) + resumeOffsetMs;
      r.temp  = currentTempC;
      r.power = currentPower;
      r.abv   = estimateABV(currentTempC);
      r.stage = (uint8_t)currentStage;
      r.valve = currentValvePos;
      sessionReadings.push_back(r);
    }
  }

  // Periodic NVS snapshot of active run (for resume after power loss)
  if (isRunning && !estopActive && (now - lastPersistUpdate >= PERSIST_RUN_MS)) {
    lastPersistUpdate = now;
    storage::saveRunSnapshot();
  }
}
