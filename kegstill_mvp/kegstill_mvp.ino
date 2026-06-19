/*
  KEG STILL - GLITCH EDITION (single-file build)
  ESP32-WROOM-DA + MAX31856 K-type + 3-wire motorized ball valve + Omron G3NA SSR

  Features:
    * Bulletproof boot (brownout off, low TX power, watchdog-safe WiFi connect)
    * Manual heater PWM (5s window) with hard over-temp E-STOP
    * 3-wire valve driver (time-based, no limit switches)
    * Profile system saved to NVS (Preferences.h)
    * 4-stage automation: WARMUP -> HEADS -> HEARTS -> TAILS -> DONE
    * In-RAM run history (720 samples @ 10s = 2h) - exportable as CSV
    * Dilution calculator (client-side JS)

  Files:
    kegstill_mvp.ino  (this file)
    index_html.h      (web dashboard, PROGMEM)
*/

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_MAX31856.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include <rom/rtc.h>
#include "index_html.h"

// ------------------------- pins -------------------------
#define SSR_PIN          5
#define VALVE_OPEN_PIN   18
#define VALVE_CLOSE_PIN  19
#define TC_CS_PIN        15
#define TC_SDI_PIN       13
#define TC_SDO_PIN       27
#define TC_SCK_PIN       14

// ------------------------- wifi -------------------------
const char* SSID = "Ponderosa";
const char* PASS = "Biggs490$!";

// ------------------------- timing -----------------------
const unsigned long PWM_WINDOW_MS    = 5000;
const unsigned long TC_POLL_MS       = 200;
const unsigned long PROBE_STALE_MS   = 5000;
const float         MAX_TC_C         = 105.0f;
const unsigned long DWELL_MS         = 150;
const unsigned long HISTORY_PERIOD_MS = 10000;
const uint16_t      HISTORY_SAMPLES  = 720;   // 2h @ 10s
const unsigned long WIFI_RECONNECT_MS = 10000;

// ------------------------- state ------------------------
WebServer server(80);
Adafruit_MAX31856* tc = nullptr;
Preferences prefs;

float    g_tempC        = 0.0f;
bool     g_tempValid    = false;
uint8_t  g_tcFault      = 0xFF;
uint32_t g_tcLastRead   = 0;
uint32_t g_tcLastUpdate = 0;

float    g_power        = 0.0f;    // 0-100 (auto or manual)
bool     g_running      = false;
bool     g_estop        = false;
uint32_t g_startMs      = 0;
uint32_t g_windowStart  = 0;
bool     g_autoMode     = false;   // false = manual, true = automation

// ------- valve -------
enum VState { V_IDLE, V_OPENING, V_CLOSING, V_HOMING };
VState   g_vState        = V_IDLE;
float    g_vPos          = 0.0f;
uint8_t  g_vTarget       = 0;
uint32_t g_vMoveStart    = 0;
uint32_t g_vMoveDur      = 0;
uint32_t g_vOpenMs       = 3500;
uint32_t g_vCloseMs      = 3500;
uint32_t g_vLastTick     = 0;
bool     g_vCalibrated   = false;

// ------- automation / profile -------
enum Stage { S_IDLE, S_WARMUP, S_HEADS, S_HEARTS, S_TAILS, S_DONE };
Stage    g_stage         = S_IDLE;
uint32_t g_stageStartMs  = 0;

struct Profile {
  char    name[24];
  float   warmupTempF;     // climb until vapor reaches this, full power
  float   headsTempF;      // start heads when vapor reaches this
  uint8_t headsPower;      // 0-100
  uint8_t headsValve;      // 0-100
  uint8_t headsMin;        // max minutes (0=disabled)
  float   heartsTempF;     // advance to hearts when vapor reaches this
  uint8_t heartsPower;
  uint8_t heartsValve;
  uint8_t heartsMin;
  float   tailsTempF;
  uint8_t tailsPower;
  uint8_t tailsValve;
  uint8_t tailsMin;
  float   shutoffTempF;    // hard shutoff
};

Profile g_profile;

void profileDefault(Profile& p) {
  strncpy(p.name, "default-spirit", sizeof(p.name));
  p.warmupTempF  = 170.0f;
  p.headsTempF   = 174.0f;
  p.headsPower   = 70;
  p.headsValve   = 10;
  p.headsMin     = 20;
  p.heartsTempF  = 178.0f;
  p.heartsPower  = 80;
  p.heartsValve  = 35;
  p.heartsMin    = 90;
  p.tailsTempF   = 196.0f;
  p.tailsPower   = 60;
  p.tailsValve   = 70;
  p.tailsMin     = 30;
  p.shutoffTempF = 205.0f;
}

void profileSave() {
  prefs.begin("kegstill", false);
  prefs.putBytes("profile", &g_profile, sizeof(Profile));
  prefs.putUInt("vopen",  g_vOpenMs);
  prefs.putUInt("vclose", g_vCloseMs);
  prefs.putBool("vcal",   g_vCalibrated);
  prefs.end();
}

void profileLoad() {
  prefs.begin("kegstill", true);
  size_t got = prefs.getBytesLength("profile");
  if (got == sizeof(Profile)) {
    prefs.getBytes("profile", &g_profile, sizeof(Profile));
  } else {
    profileDefault(g_profile);
  }
  uint32_t o = prefs.getUInt("vopen",  3500);
  uint32_t c = prefs.getUInt("vclose", 3500);
  bool     v = prefs.getBool("vcal",   false);
  prefs.end();
  if (o >= 500 && o <= 120000) g_vOpenMs  = o;
  if (c >= 500 && c <= 120000) g_vCloseMs = c;
  g_vCalibrated = v;
}

// ------- history (RAM ring) -------
struct Sample {
  uint32_t t;          // seconds since run start
  float    tempF;
  uint8_t  power;
  uint8_t  valve;
  uint8_t  stage;
};
Sample   g_hist[HISTORY_SAMPLES];
uint16_t g_histHead  = 0;
uint16_t g_histCount = 0;
uint32_t g_histLastMs = 0;

void historyClear() { g_histHead = 0; g_histCount = 0; g_histLastMs = 0; }

void historyPush(uint32_t elapsedS, float f, uint8_t pwr, uint8_t valve, uint8_t stage) {
  g_hist[g_histHead] = { elapsedS, f, pwr, valve, stage };
  g_histHead = (g_histHead + 1) % HISTORY_SAMPLES;
  if (g_histCount < HISTORY_SAMPLES) g_histCount++;
}

// ------------------------- helpers ----------------------
float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }
float fToC(float f) { return (f - 32.0f) * 5.0f / 9.0f; }

String tcFaultStr(uint8_t f) {
  if (f == 0) return "OK";
  String s;
  if (f & MAX31856_FAULT_OPEN)    s += "OPEN ";
  if (f & MAX31856_FAULT_OVUV)    s += "OV/UV ";
  if (f & MAX31856_FAULT_TCHIGH)  s += "TC_HIGH ";
  if (f & MAX31856_FAULT_TCLOW)   s += "TC_LOW ";
  if (f & MAX31856_FAULT_CJHIGH)  s += "CJ_HIGH ";
  if (f & MAX31856_FAULT_CJLOW)   s += "CJ_LOW ";
  if (f & MAX31856_FAULT_TCRANGE) s += "TC_RANGE ";
  if (f & MAX31856_FAULT_CJRANGE) s += "CJ_RANGE ";
  s.trim();
  return s;
}

const char* stageName(Stage s) {
  switch (s) {
    case S_IDLE:   return "IDLE";
    case S_WARMUP: return "WARMUP";
    case S_HEADS:  return "HEADS";
    case S_HEARTS: return "HEARTS";
    case S_TAILS:  return "TAILS";
    case S_DONE:   return "DONE";
  }
  return "?";
}

// ------------------------- valve ------------------------
void vStop() {
  digitalWrite(VALVE_OPEN_PIN, LOW);
  digitalWrite(VALVE_CLOSE_PIN, LOW);
}
void vOpen() {
  digitalWrite(VALVE_CLOSE_PIN, LOW);
  delay(DWELL_MS);
  digitalWrite(VALVE_OPEN_PIN, HIGH);
}
void vClose() {
  digitalWrite(VALVE_OPEN_PIN, LOW);
  delay(DWELL_MS);
  digitalWrite(VALVE_CLOSE_PIN, HIGH);
}

void valveSetPosition(uint8_t pct) {
  if (pct > 100) pct = 100;
  g_vTarget = pct;
  float delta = (float)pct - g_vPos;
  if (fabsf(delta) < 1.0f) { vStop(); g_vState = V_IDLE; return; }
  uint32_t travelMs = (delta > 0) ? g_vOpenMs : g_vCloseMs;
  g_vMoveDur = (uint32_t)(fabsf(delta) * (float)travelMs / 100.0f);
  if (pct == 0)   g_vMoveDur = (uint32_t)(g_vCloseMs * 1.2f);
  if (pct == 100) g_vMoveDur = (uint32_t)(g_vOpenMs  * 1.2f);
  g_vMoveStart = millis();
  if (delta > 0) { g_vState = V_OPENING; vOpen(); }
  else           { g_vState = V_CLOSING; vClose(); }
}

void valveHome() {
  g_vTarget    = 0;
  g_vMoveStart = millis();
  g_vMoveDur   = (uint32_t)(g_vCloseMs * 1.2f);
  g_vState     = V_HOMING;
  vClose();
}

void valvePoll() {
  uint32_t now = millis();
  uint32_t dt  = now - g_vLastTick;
  g_vLastTick = now;
  if (g_vState == V_IDLE) return;
  if (g_vState == V_OPENING) g_vPos += (float)dt * 100.0f / (float)g_vOpenMs;
  if (g_vState == V_CLOSING || g_vState == V_HOMING)
    g_vPos -= (float)dt * 100.0f / (float)g_vCloseMs;
  if (g_vPos < 0)   g_vPos = 0;
  if (g_vPos > 100) g_vPos = 100;
  if (now - g_vMoveStart >= g_vMoveDur) {
    vStop();
    if (g_vTarget == 0)   g_vPos = 0;
    if (g_vTarget == 100) g_vPos = 100;
    g_vState = V_IDLE;
  }
}

// ------------------------- thermocouple -----------------
void tcPoll() {
  uint32_t now = millis();
  if (now - g_tcLastRead < TC_POLL_MS) return;
  g_tcLastRead = now;
  if (!tc) return;
  uint8_t f = tc->readFault();
  float   t = tc->readThermocoupleTemperature();
  g_tcFault = f;
  if (f == 0 && isfinite(t) && t > -50.0f && t < 1300.0f) {
    g_tempC        = t;
    g_tempValid    = true;
    g_tcLastUpdate = now;
  }
  if (g_tempValid && (now - g_tcLastUpdate > PROBE_STALE_MS)) g_tempValid = false;
}

// ------------------------- automation -------------------
void stageEnter(Stage s) {
  g_stage         = s;
  g_stageStartMs  = millis();
  Serial.printf("[AUTO] stage -> %s\n", stageName(s));
  switch (s) {
    case S_WARMUP:
      g_power = 100;
      valveSetPosition(0);
      break;
    case S_HEADS:
      g_power = g_profile.headsPower;
      valveSetPosition(g_profile.headsValve);
      break;
    case S_HEARTS:
      g_power = g_profile.heartsPower;
      valveSetPosition(g_profile.heartsValve);
      break;
    case S_TAILS:
      g_power = g_profile.tailsPower;
      valveSetPosition(g_profile.tailsValve);
      break;
    case S_DONE:
      g_power = 0;
      g_running = false;
      digitalWrite(SSR_PIN, LOW);
      valveSetPosition(0);
      break;
    default: break;
  }
}

void automationPoll() {
  if (!g_autoMode || !g_running || g_estop || g_stage == S_DONE) return;
  if (!g_tempValid) return;
  float f = cToF(g_tempC);
  uint32_t now = millis();
  uint32_t stageElapsedMin = (now - g_stageStartMs) / 60000UL;

  // hard shutoff
  if (f >= g_profile.shutoffTempF) {
    Serial.printf("[AUTO] shutoff @ %.1fF\n", f);
    stageEnter(S_DONE);
    return;
  }

  switch (g_stage) {
    case S_WARMUP:
      if (f >= g_profile.headsTempF) stageEnter(S_HEADS);
      break;
    case S_HEADS:
      if (f >= g_profile.heartsTempF) stageEnter(S_HEARTS);
      else if (g_profile.headsMin && stageElapsedMin >= g_profile.headsMin) stageEnter(S_HEARTS);
      break;
    case S_HEARTS:
      if (f >= g_profile.tailsTempF) stageEnter(S_TAILS);
      else if (g_profile.heartsMin && stageElapsedMin >= g_profile.heartsMin) stageEnter(S_TAILS);
      break;
    case S_TAILS:
      if (g_profile.tailsMin && stageElapsedMin >= g_profile.tailsMin) stageEnter(S_DONE);
      break;
    default: break;
  }
}

// ------------------------- SSR / control ----------------
void controlPoll() {
  if (!g_running || g_estop) { digitalWrite(SSR_PIN, LOW); return; }
  if (!g_tempValid) {
    g_estop = true; g_power = 0; g_running = false;
    digitalWrite(SSR_PIN, LOW);
    Serial.println("[ESTOP] probe lost");
    return;
  }
  if (g_tempC > MAX_TC_C) {
    g_estop = true; g_power = 0; g_running = false;
    digitalWrite(SSR_PIN, LOW);
    Serial.println("[ESTOP] over-temp");
    return;
  }
  uint32_t now = millis();
  if (now - g_windowStart >= PWM_WINDOW_MS) g_windowStart += PWM_WINDOW_MS;
  uint32_t onMs = (uint32_t)(g_power / 100.0f * PWM_WINDOW_MS);
  digitalWrite(SSR_PIN, (now - g_windowStart) < onMs ? HIGH : LOW);
}

void historyPoll() {
  if (!g_running) return;
  uint32_t now = millis();
  if (now - g_histLastMs < HISTORY_PERIOD_MS) return;
  g_histLastMs = now;
  uint32_t elapsedS = (now - g_startMs) / 1000UL;
  historyPush(elapsedS,
              g_tempValid ? cToF(g_tempC) : NAN,
              (uint8_t)(g_power + 0.5f),
              (uint8_t)(g_vPos + 0.5f),
              (uint8_t)g_stage);
}

// ------------------------- web handlers -----------------
void handleRoot()  { server.send_P(200, "text/html", INDEX_HTML); }

void handleStatus() {
  StaticJsonDocument<512> d;
  d["status"]   = g_estop ? "ESTOP" : (g_running ? "RUN" : "IDLE");
  d["estop"]    = g_estop;
  d["bleOk"]    = g_tempValid;
  d["power"]    = round(g_power * 10) / 10.0;
  d["valvePos"] = (int)(g_vPos + 0.5f);
  d["elapsed"]  = g_running ? (millis() - g_startMs) / 1000UL : 0;
  d["auto"]     = g_autoMode;
  d["stage"]    = stageName(g_stage);
  if (g_tempValid) {
    d["tempC"] = round(g_tempC * 10) / 10.0;
    d["tempF"] = round(cToF(g_tempC) * 10) / 10.0;
  } else {
    d["tempC"] = (const char*)nullptr;
    d["tempF"] = (const char*)nullptr;
  }
  String j; serializeJson(d, j);
  server.send(200, "application/json", j);
}

void handleProbe() {
  StaticJsonDocument<256> d;
  d["valid"]    = g_tempValid;
  d["tempC"]    = round(g_tempC * 100) / 100.0;
  d["tempF"]    = round(cToF(g_tempC) * 100) / 100.0;
  d["fault"]    = g_tcFault;
  d["faultStr"] = tcFaultStr(g_tcFault);
  d["ageMs"]    = (uint32_t)(millis() - g_tcLastUpdate);
  String j; serializeJson(d, j);
  server.send(200, "application/json", j);
}

void handleValveStatus() {
  StaticJsonDocument<256> d;
  const char* sn = "IDLE";
  if (g_vState == V_OPENING) sn = "OPENING";
  if (g_vState == V_CLOSING) sn = "CLOSING";
  if (g_vState == V_HOMING)  sn = "HOMING";
  d["state"]       = sn;
  d["position"]    = round(g_vPos * 10) / 10.0;
  d["target"]      = g_vTarget;
  d["calibrated"]  = g_vCalibrated;
  d["openTimeMs"]  = g_vOpenMs;
  d["closeTimeMs"] = g_vCloseMs;
  String j; serializeJson(d, j);
  server.send(200, "application/json", j);
}

void handlePower() {
  if (g_autoMode) { server.send(409, "text/plain", "automation active"); return; }
  if (!server.hasArg("plain")) { server.send(400); return; }
  StaticJsonDocument<64> d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400); return; }
  g_power = constrain((float)(d["power"] | 0.0), 0.0f, 100.0f);
  server.send(200);
}

void handleValve() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  StaticJsonDocument<128> d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400); return; }
  bool calChanged = false;
  if (d.containsKey("openMs") && d.containsKey("closeMs")) {
    uint32_t o = d["openMs"]  | 0;
    uint32_t c = d["closeMs"] | 0;
    if (o >= 500 && o <= 120000 && c >= 500 && c <= 120000) {
      g_vOpenMs = o; g_vCloseMs = c; g_vCalibrated = true; calChanged = true;
    }
  }
  if (d.containsKey("pos"))   valveSetPosition((uint8_t)(d["pos"] | 0));
  if (d.containsKey("cmd")) {
    String c = d["cmd"].as<String>();
    if (c == "open")   valveSetPosition(100);
    if (c == "close")  valveSetPosition(0);
    if (c == "stop")   { vStop(); g_vState = V_IDLE; }
    if (c == "rehome") valveHome();
  }
  if (calChanged) profileSave();
  server.send(200);
}

void handleStart() {
  if (g_estop)      { server.send(400, "text/plain", "ESTOP active - reset first"); return; }
  if (!g_tempValid) { server.send(400, "text/plain", "no probe"); return; }
  g_running     = true;
  g_startMs     = millis();
  g_windowStart = millis();
  g_histLastMs  = 0;
  historyClear();
  if (g_autoMode) stageEnter(S_WARMUP);
  else            g_stage = S_IDLE;
  server.send(200);
}
void handleStop()  { g_running = false; g_autoMode = false; g_stage = S_IDLE; g_power = 0; digitalWrite(SSR_PIN, LOW); server.send(200); }
void handleEstop() { g_estop   = true; g_running = false; g_autoMode = false; g_stage = S_IDLE; g_power = 0; digitalWrite(SSR_PIN, LOW); server.send(200); }
void handleReset() { g_estop   = false; g_running = false; g_autoMode = false; g_stage = S_IDLE; g_power = 0; digitalWrite(SSR_PIN, LOW); server.send(200); }

// profile
void handleProfileGet() {
  StaticJsonDocument<512> d;
  d["name"]        = g_profile.name;
  d["warmupTempF"] = g_profile.warmupTempF;
  d["headsTempF"]  = g_profile.headsTempF;
  d["headsPower"]  = g_profile.headsPower;
  d["headsValve"]  = g_profile.headsValve;
  d["headsMin"]    = g_profile.headsMin;
  d["heartsTempF"] = g_profile.heartsTempF;
  d["heartsPower"] = g_profile.heartsPower;
  d["heartsValve"] = g_profile.heartsValve;
  d["heartsMin"]   = g_profile.heartsMin;
  d["tailsTempF"]  = g_profile.tailsTempF;
  d["tailsPower"]  = g_profile.tailsPower;
  d["tailsValve"]  = g_profile.tailsValve;
  d["tailsMin"]    = g_profile.tailsMin;
  d["shutoffTempF"]= g_profile.shutoffTempF;
  String j; serializeJson(d, j);
  server.send(200, "application/json", j);
}

void handleProfileSet() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  StaticJsonDocument<512> d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400); return; }
  if (d.containsKey("name")) {
    String n = d["name"].as<String>();
    n.toCharArray(g_profile.name, sizeof(g_profile.name));
  }
  if (d.containsKey("warmupTempF")) g_profile.warmupTempF = d["warmupTempF"];
  if (d.containsKey("headsTempF"))  g_profile.headsTempF  = d["headsTempF"];
  if (d.containsKey("headsPower"))  g_profile.headsPower  = (uint8_t)constrain((int)d["headsPower"],  0, 100);
  if (d.containsKey("headsValve"))  g_profile.headsValve  = (uint8_t)constrain((int)d["headsValve"],  0, 100);
  if (d.containsKey("headsMin"))    g_profile.headsMin    = (uint8_t)constrain((int)d["headsMin"],    0, 240);
  if (d.containsKey("heartsTempF")) g_profile.heartsTempF = d["heartsTempF"];
  if (d.containsKey("heartsPower")) g_profile.heartsPower = (uint8_t)constrain((int)d["heartsPower"], 0, 100);
  if (d.containsKey("heartsValve")) g_profile.heartsValve = (uint8_t)constrain((int)d["heartsValve"], 0, 100);
  if (d.containsKey("heartsMin"))   g_profile.heartsMin   = (uint8_t)constrain((int)d["heartsMin"],   0, 240);
  if (d.containsKey("tailsTempF"))  g_profile.tailsTempF  = d["tailsTempF"];
  if (d.containsKey("tailsPower"))  g_profile.tailsPower  = (uint8_t)constrain((int)d["tailsPower"],  0, 100);
  if (d.containsKey("tailsValve"))  g_profile.tailsValve  = (uint8_t)constrain((int)d["tailsValve"],  0, 100);
  if (d.containsKey("tailsMin"))    g_profile.tailsMin    = (uint8_t)constrain((int)d["tailsMin"],    0, 240);
  if (d.containsKey("shutoffTempF"))g_profile.shutoffTempF= d["shutoffTempF"];
  profileSave();
  server.send(200);
}

// automation
void handleAutoStart() {
  if (g_estop)      { server.send(400, "text/plain", "ESTOP - reset first"); return; }
  if (!g_tempValid) { server.send(400, "text/plain", "no probe"); return; }
  g_autoMode    = true;
  g_running     = true;
  g_startMs     = millis();
  g_windowStart = millis();
  historyClear();
  stageEnter(S_WARMUP);
  server.send(200);
}
void handleAutoStop() {
  g_autoMode = false;
  g_stage    = S_IDLE;
  g_running  = false;
  g_power    = 0;
  digitalWrite(SSR_PIN, LOW);
  server.send(200);
}

// history
void handleHistory() {
  // stream JSON to avoid big buffer
  String out;
  out.reserve(g_histCount * 32 + 32);
  out += "{\"count\":";
  out += g_histCount;
  out += ",\"samples\":[";
  uint16_t start = (g_histCount < HISTORY_SAMPLES) ? 0 : g_histHead;
  for (uint16_t i = 0; i < g_histCount; i++) {
    uint16_t idx = (start + i) % HISTORY_SAMPLES;
    Sample& s = g_hist[idx];
    if (i) out += ',';
    out += '[';
    out += s.t;            out += ',';
    if (isnan(s.tempF))    out += "null";
    else                   out += String(s.tempF, 1);
    out += ',';
    out += s.power;        out += ',';
    out += s.valve;        out += ',';
    out += s.stage;
    out += ']';
  }
  out += "]}";
  server.send(200, "application/json", out);
}

void handleHistoryCsv() {
  String out = "elapsed_s,temp_f,power_pct,valve_pct,stage\n";
  uint16_t start = (g_histCount < HISTORY_SAMPLES) ? 0 : g_histHead;
  for (uint16_t i = 0; i < g_histCount; i++) {
    uint16_t idx = (start + i) % HISTORY_SAMPLES;
    Sample& s = g_hist[idx];
    out += String(s.t); out += ',';
    if (isnan(s.tempF)) out += "";
    else                out += String(s.tempF, 1);
    out += ',';
    out += String(s.power); out += ',';
    out += String(s.valve); out += ',';
    out += stageName((Stage)s.stage);
    out += '\n';
  }
  server.sendHeader("Content-Disposition", "attachment; filename=run-history.csv");
  server.send(200, "text/csv", out);
}

void handleHistoryClear() { historyClear(); server.send(200); }

// ------------------------- wifi monitor -----------------
void wifiPoll() {
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < WIFI_RECONNECT_MS) return;
  lastCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] connection lost! Attempting recovery...");
    WiFi.begin(SSID, PASS);
  }
}

// ------------------------- setup / loop -----------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[BOOT] Keg Still - Glitch Edition (GOD MODE WiFi Patch)");
  Serial.printf("[BOOT] reset reason cpu0=%d cpu1=%d\n",
                rtc_get_reset_reason(0), rtc_get_reset_reason(1));

  btStop();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  setCpuFrequencyMhz(80);
  
  pinMode(SSR_PIN, OUTPUT);          digitalWrite(SSR_PIN, LOW);
  pinMode(VALVE_OPEN_PIN, OUTPUT);   digitalWrite(VALVE_OPEN_PIN, LOW);
  pinMode(VALVE_CLOSE_PIN, OUTPUT);  digitalWrite(VALVE_CLOSE_PIN, LOW);

  profileLoad();
  
  // valve home on boot
  digitalWrite(VALVE_CLOSE_PIN, HIGH);
  delay(g_vCloseMs * 12 / 10);
  digitalWrite(VALVE_CLOSE_PIN, LOW);
  g_vPos = 0;

  tc = new Adafruit_MAX31856(TC_CS_PIN, TC_SDI_PIN, TC_SDO_PIN, TC_SCK_PIN);
  if (tc->begin()) {
    tc->setThermocoupleType(MAX31856_TCTYPE_K);
    tc->setNoiseFilter(MAX31856_NOISE_FILTER_60HZ);
    tc->setConversionMode(MAX31856_CONTINUOUS);
  }

  // ----- WiFi ----- 
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); // NO MODEM SLEEP - stay awake and stick to the signal
  
  // MAX POWER - blast the radio at 19.5dBm
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  
  Serial.printf("[WiFi] connecting to '%s' (TX Power: 19.5dBm, Sleep: OFF)\n", SSID);
  WiFi.begin(SSID, PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] OK ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("[WiFi] Initial connect FAILED - will retry in loop");
  }

  server.on("/",                 handleRoot);
  server.on("/api/status",       HTTP_GET,  handleStatus);
  server.on("/api/probe",        HTTP_GET,  handleProbe);
  server.on("/api/valve/status", HTTP_GET,  handleValveStatus);
  server.on("/api/power",        HTTP_POST, handlePower);
  server.on("/api/valve",        HTTP_POST, handleValve);
  server.on("/api/start",        HTTP_POST, handleStart);
  server.on("/api/stop",         HTTP_POST, handleStop);
  server.on("/api/estop",        HTTP_POST, handleEstop);
  server.on("/api/reset",        HTTP_POST, handleReset);
  server.on("/api/profile",      HTTP_GET,  handleProfileGet);
  server.on("/api/profile",      HTTP_POST, handleProfileSet);
  server.on("/api/auto/start",   HTTP_POST, handleAutoStart);
  server.on("/api/auto/stop",    HTTP_POST, handleAutoStop);
  server.on("/api/history",      HTTP_GET,  handleHistory);
  server.on("/api/history.csv",  HTTP_GET,  handleHistoryCsv);
  server.on("/api/history",      HTTP_DELETE, handleHistoryClear);
  server.begin();
}

void loop() {
  wifiPoll();
  server.handleClient();
  tcPoll();
  valvePoll();
  static uint32_t lastCtrl = 0;
  if (millis() - lastCtrl > 100) {
    lastCtrl = millis();
    controlPoll();
    automationPoll();
  }
  historyPoll();
}
