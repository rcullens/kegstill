/*
  KEG STILL - GLITCH EDITION - SPLIT BUILD
  STABILITY PATCH v3.2 (The 'Weak Power' Defense)
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <rom/rtc.h>

#include "config.h"
#include "state.h"
#include "storage.h"
#include "thermocouple.h"
#include "control.h"
#include "web_handlers.h"
#include "valve.h"
#include "history.h"

const char* WIFI_SSID_DEFAULT = "Ponderosa";
const char* WIFI_PASS_DEFAULT = "Biggs490$!";
const char* OTA_HOSTNAME      = "kegstill-cq60";
const char* OTA_PASSWORD      = "kegstill";

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

String stageName(Stage s) {
  switch (s) {
    case STAGE_IDLE:     return "IDLE";
    case STAGE_HEATUP:   return "HEATUP";
    case STAGE_HEADS:    return "HEADS";
    case STAGE_HEARTS:   return "HEARTS";
    case STAGE_TAILS:    return "TAILS";
    case STAGE_SHUTDOWN: return "SHUTDOWN";
    default:             return "?";
  }
}

float estimateABV(float tempC) {
  if (tempC < 78.0f)  return 96.0f;
  if (tempC > 99.5f)  return 8.0f;
  return 95.0f - (tempC - 78.0f) * 4.0f;
}

static void createDefaultProfiles() {
  profiles.clear();
  profiles.push_back({"Stripping Run", 180.0f, 100.0f, 6.0f, 200.0f,
                      100.0f, 60.0f, 300,  185.0f, 80.0f,
                      100, 100, 100, 100});
}

static bool connectWiFi() {
  String ssid, pass;
  if (!storage::loadWifi(ssid, pass) || ssid.length() == 0) {
    ssid = WIFI_SSID_DEFAULT; pass = WIFI_PASS_DEFAULT;
  }

  Serial.println("[WiFi] Entering STA Mode..."); Serial.flush();
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); 

  // ABSOLUTE MINIMUM POWER (8.5dBm) - IF IT STILL BROWNS OUT, YOUR POWER IS TRASH
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.printf("[WiFi] Connecting to '%s' (LOW POWER MODE)...", ssid.c_str()); Serial.flush();
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(500); Serial.print('.'); Serial.flush();
  }
  Serial.println();
  return (WiFi.status() == WL_CONNECTED);
}

void setup() {
  // KILL RADIO ON BOOT TO SAVE POWER
  WiFi.mode(WIFI_OFF);
  
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n[BOOT] Keg Still GLITCH (v3.2 - ULTIMATE STABILITY)");
  Serial.printf("[BOOT] Reset CPU0: %d, CPU1: %d\n", rtc_get_reset_reason(0), rtc_get_reset_reason(1));
  Serial.flush();

  Serial.println("[SYS] Filesystem Init..."); Serial.flush();
  storage::begin(); 

  Serial.println("[SYS] Subsystems Init..."); Serial.flush();
  control::begin();
  valve::begin();   
  history::begin();

  if (!storage::loadProfiles()) {
    createDefaultProfiles(); storage::saveProfiles();
  }
  currentProfileIndex = storage::loadCurrentProfileIndex();
  automationEnabled = storage::loadAutomation();
  storage::loadBatch();
  storage::loadRunSnapshot();

  Serial.println("[SYS] Init Thermocouple..."); Serial.flush();
  thermocouple::begin();

  // EXPERIMENTAL: 30 SECOND DELAY TO LET YOU SEE THE SENSOR BEFORE WIFI KILLS IT
  Serial.println("\n[TEST] STANDBY FOR 30 SECONDS... WATCH FOR BROWNOUTS!");
  for(int i=30; i>0; i--) {
    Serial.printf("[TEST] Radio keys up in %d seconds... Temp: %.2fC\n", i, currentTempC);
    Serial.flush();
    thermocouple::poll();
    delay(1000);
  }

  Serial.println("\n[SYS] STARTING WIFI RADIO NOW - GOOD LUCK..."); Serial.flush();
  if (connectWiFi()) {
    Serial.printf("[WiFi] OK ip=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] FAILED - RETRYING IN LOOP");
  }
  Serial.flush();

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
  
  web_handlers::registerRoutes(server);
  server.begin();
  Serial.printf("[READY] http://%s\n", WiFi.localIP().toString().c_str()); Serial.flush();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  valve::poll();
  thermocouple::poll();

  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED && (now % 15000 < 50)) {
    WiFi.begin();
  }

  if (now - lastControlUpdate >= CONTROL_PERIOD_MS) {
    lastControlUpdate = now; control::update();
  }

  if (now - lastStatusUpdate >= STATUS_PERIOD_MS) {
    lastStatusUpdate = now;
    if (isRunning && !estopActive && sessionReadings.size() < MAX_SESSION_PTS) {
      Reading r; r.ts = (now - startMillis) + resumeOffsetMs;
      r.temp = currentTempC; r.power = currentPower;
      r.abv = estimateABV(currentTempC); r.stage = (uint8_t)currentStage;
      r.valve = currentValvePos; sessionReadings.push_back(r);
    }
  }

  if (isRunning && !estopActive && (now - lastPersistUpdate >= PERSIST_RUN_MS)) {
    lastPersistUpdate = now; storage::saveRunSnapshot();
  }
}
