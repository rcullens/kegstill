/*
  KEG STILL - MVP (monolithic, fresh start)
  ESP32-WROOM-DA + MAX31856 K-type + 3-wire valve + Omron G3NA SSR
  Single file + index_html.h. Boot-loop-proof: brownout off, no static driver
  objects, no NVS/LittleFS, no BLE, no OTA.
*/

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Adafruit_MAX31856.h>
#include "index_html.h"

// ---------- pins ----------
#define SSR_PIN          5
#define VALVE_OPEN_PIN   18
#define VALVE_CLOSE_PIN  19
#define TC_CS_PIN        15
#define TC_SDI_PIN       13
#define TC_SDO_PIN       27
#define TC_SCK_PIN       14

// ---------- wifi ----------
const char* SSID = "Ponderosa";
const char* PASS = "Biggs490$!";

// ---------- timing ----------
const unsigned long PWM_WINDOW_MS  = 5000;
const unsigned long TC_POLL_MS     = 200;
const unsigned long PROBE_STALE_MS = 5000;
const unsigned long MAX_TC_C       = 105;  // hard ceiling
const unsigned long DWELL_MS       = 150;  // reverse dwell

// ---------- state ----------
WebServer server(80);
Adafruit_MAX31856* tc = nullptr;

float    g_tempC        = 0.0f;
bool     g_tempValid    = false;
uint8_t  g_tcFault      = 0xFF;
uint32_t g_tcLastRead   = 0;
uint32_t g_tcLastUpdate = 0;

float    g_power        = 0.0f;   // 0-100, manual
bool     g_running      = false;
bool     g_estop        = false;
uint32_t g_startMs      = 0;
uint32_t g_windowStart  = 0;

// valve
enum VState { V_IDLE, V_OPENING, V_CLOSING, V_HOMING };
VState   g_vState        = V_IDLE;
float    g_vPos          = 0.0f;    // 0-100
uint8_t  g_vTarget       = 0;
uint32_t g_vMoveStart    = 0;
uint32_t g_vMoveDur      = 0;
uint32_t g_vOpenMs       = 3500;
uint32_t g_vCloseMs      = 3500;
uint32_t g_vLastTick     = 0;
bool     g_vCalibrated   = false;

// ---------- helpers ----------
float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }

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

// ---------- valve ----------
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
  if (g_vPos < 0) g_vPos = 0;
  if (g_vPos > 100) g_vPos = 100;
  if (now - g_vMoveStart >= g_vMoveDur) {
    vStop();
    if (g_vTarget == 0)   g_vPos = 0;
    if (g_vTarget == 100) g_vPos = 100;
    g_vState = V_IDLE;
  }
}

// ---------- thermocouple ----------
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
  static uint32_t lastLog = 0;
  if (now - lastLog > 2000) {
    lastLog = now;
    if (g_tempValid) Serial.printf("[TC] %.2fC (%.1fF) fault=0x00\n", g_tempC, cToF(g_tempC));
    else             Serial.printf("[TC] INVALID fault=0x%02X (%s)\n", f, tcFaultStr(f).c_str());
  }
}

// ---------- control / SSR ----------
void controlPoll() {
  if (!g_running || g_estop) { digitalWrite(SSR_PIN, LOW); return; }
  if (!g_tempValid)              { g_estop = true; g_power = 0; digitalWrite(SSR_PIN, LOW); g_running = false; return; }
  if (g_tempC > MAX_TC_C)        { g_estop = true; g_power = 0; digitalWrite(SSR_PIN, LOW); g_running = false; Serial.println("[ESTOP] over-temp"); return; }
  uint32_t now = millis();
  if (now - g_windowStart >= PWM_WINDOW_MS) g_windowStart += PWM_WINDOW_MS;
  uint32_t onMs = (uint32_t)(g_power / 100.0f * PWM_WINDOW_MS);
  digitalWrite(SSR_PIN, (now - g_windowStart) < onMs ? HIGH : LOW);
}

// ---------- web handlers ----------
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleStatus() {
  StaticJsonDocument<384> d;
  d["status"]   = g_estop ? "ESTOP" : (g_running ? "RUN" : "IDLE");
  d["estop"]    = g_estop;
  d["bleOk"]    = g_tempValid;          // dashboard uses this name
  d["power"]    = g_power;
  d["valvePos"] = (int)(g_vPos + 0.5f);
  d["elapsed"]  = g_running ? (millis() - g_startMs) / 1000UL : 0;
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
  d["state"]      = sn;
  d["position"]   = round(g_vPos * 10) / 10.0;
  d["target"]     = g_vTarget;
  d["calibrated"] = g_vCalibrated;
  d["openTimeMs"] = g_vOpenMs;
  d["closeTimeMs"]= g_vCloseMs;
  String j; serializeJson(d, j);
  server.send(200, "application/json", j);
}

void handlePower() {
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
  if (d.containsKey("openMs") && d.containsKey("closeMs")) {
    uint32_t o = d["openMs"]  | 0;
    uint32_t c = d["closeMs"] | 0;
    if (o >= 500 && o <= 120000 && c >= 500 && c <= 120000) {
      g_vOpenMs = o; g_vCloseMs = c; g_vCalibrated = true;
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
  server.send(200);
}

void handleStart() {
  if (g_estop)      { server.send(400, "text/plain", "ESTOP active - reset first"); return; }
  if (!g_tempValid) { server.send(400, "text/plain", "no probe"); return; }
  g_running     = true;
  g_startMs     = millis();
  g_windowStart = millis();
  server.send(200);
}
void handleStop()  { g_running = false; g_power = 0; digitalWrite(SSR_PIN, LOW); server.send(200); }
void handleEstop() { g_estop   = true; g_running = false; g_power = 0; digitalWrite(SSR_PIN, LOW); server.send(200); }
void handleReset() { g_estop = false; g_running = false; g_power = 0; digitalWrite(SSR_PIN, LOW); server.send(200); }

// ---------- setup / loop ----------
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[BOOT] Keg Still MVP");

  pinMode(SSR_PIN, OUTPUT);          digitalWrite(SSR_PIN, LOW);
  pinMode(VALVE_OPEN_PIN, OUTPUT);   digitalWrite(VALVE_OPEN_PIN, LOW);
  pinMode(VALVE_CLOSE_PIN, OUTPUT);  digitalWrite(VALVE_CLOSE_PIN, LOW);

  // valve home on boot (use internal limit absorb)
  Serial.println("[VALVE] homing closed...");
  digitalWrite(VALVE_CLOSE_PIN, HIGH);
  delay(g_vCloseMs * 12 / 10);   // closeMs * 1.2
  digitalWrite(VALVE_CLOSE_PIN, LOW);
  g_vPos = 0;
  Serial.println("[VALVE] homed");

  // thermocouple - allocated dynamically, NOT a global static
  tc = new Adafruit_MAX31856(TC_CS_PIN, TC_SDI_PIN, TC_SDO_PIN, TC_SCK_PIN);
  if (!tc->begin()) Serial.println("[TC] init FAILED - check SPI wires");
  else              Serial.println("[TC] init OK");
  tc->setThermocoupleType(MAX31856_TCTYPE_K);
  tc->setNoiseFilter(MAX31856_NOISE_FILTER_60HZ);
  tc->setConversionMode(MAX31856_CONTINUOUS);

  // wifi
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(SSID, PASS);
  Serial.printf("[WiFi] connecting to '%s'", SSID);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) { delay(400); Serial.print('.'); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[WiFi] FAILED - reboot"); delay(3000); ESP.restart(); }
  Serial.printf("[WiFi] OK ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  delay(2000);

  // routes
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
  server.begin();
  Serial.printf("[READY] http://%s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  server.handleClient();
  tcPoll();
  valvePoll();
  static uint32_t lastCtrl = 0;
  if (millis() - lastCtrl > 100) { lastCtrl = millis(); controlPoll(); }
}
