// web_handlers.cpp
#include "web_handlers.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "control.h"
#include "valve.h"
#include "history.h"
#include "ble_scanner.h"
#include "index_html.h"
#include <ArduinoJson.h>

static WebServer* S = nullptr;

static void handleRoot() {
  S->send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  StaticJsonDocument<640> doc;
  doc["power"]      = currentPower;
  unsigned long elapsed = isRunning ? ((millis() - startMillis) / 1000UL) + (resumeOffsetMs / 1000UL) : 0;
  doc["elapsed"]    = elapsed;
  doc["status"]     = estopActive ? "ESTOP" : (isRunning ? (automationEnabled ? "AUTO" : "MANUAL") : "IDLE");
  doc["stage"]      = stageName(currentStage);
  doc["stageIdx"]   = (uint8_t)currentStage;
  doc["targetTemp"] = (profiles.size() > 0 && currentProfileIndex < (int)profiles.size())
                     ? (int)profiles[currentProfileIndex].targetTemp : 180;
  doc["profileIdx"] = currentProfileIndex;
  doc["automation"] = automationEnabled;
  doc["estop"]      = estopActive;
  doc["bleOk"]      = bleTempValid;
  doc["valvePos"]   = currentValvePos;
  doc["valveAuto"]  = valveAutoFollowStage;
  doc["resumePending"] = resumePending;
  doc["resumeElapsed"] = (uint32_t)resumeElapsedSec;

  if (bleTempValid) {
    doc["tempF"] = round(cToF(currentTempC) * 10) / 10.0;
    doc["tempC"] = round(currentTempC * 10) / 10.0;
    doc["abv"]   = estimateABV(currentTempC);
  } else {
    doc["tempF"] = (const char*)nullptr;
    doc["tempC"] = (const char*)nullptr;
    doc["abv"]   = (const char*)nullptr;  // no probe = no fake ABV
  }
  String out; serializeJson(doc, out);
  S->send(200, "application/json", out);
}

static void handleStart() {
  bool resume = false;
  if (S->hasArg("plain")) {
    StaticJsonDocument<64> d;
    if (deserializeJson(d, S->arg("plain")) == DeserializationError::Ok) {
      resume = d["resume"] | false;
    }
  }
  if (resume && !resumePending) resume = false;
  bool ok = control::startRun(resume);
  if (!ok) { S->send(400, "application/json", "{\"error\":\"cannot start: no BLE probe or estop\"}"); return; }
  resumePending = false;
  S->send(200, "application/json", "{\"ok\":true}");
}

static void handleStop()  { control::stopRun(); S->send(200); }
static void handleEStop() { control::estop();   S->send(200); }
static void handleReset() { control::resetAll(); S->send(200); }

static void handlePower() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  control::setManualPower(doc["power"] | currentPower);
  S->send(200);
}

static void handleAutomation() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  control::setAutomation(doc["enabled"] | false);
  S->send(200);
}

static void handleAdvanceStage() { control::advanceStage(); S->send(200); }

static void serializeProfile(JsonObject o, const Profile& p) {
  o["name"]            = p.name;
  o["targetTemp"]      = p.targetTemp;
  o["maxPower"]        = p.maxPower;
  o["kp"]              = p.kp;
  o["cutTemp"]         = p.cutTemp;
  o["heatupPower"]     = p.heatupPower;
  o["headsPower"]      = p.headsPower;
  o["headsDuration_s"] = p.headsDuration_s;
  o["tailsTemp"]       = p.tailsTemp;
  o["tailsPower"]      = p.tailsPower;
  o["valveHeatup"]     = p.valveHeatup;
  o["valveHeads"]      = p.valveHeads;
  o["valveHearts"]     = p.valveHearts;
  o["valveTails"]      = p.valveTails;
}

static void handleProfiles() {
  StaticJsonDocument<3072> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto& p : profiles) serializeProfile(arr.createNestedObject(), p);
  String out; serializeJson(doc, out);
  S->send(200, "application/json", out);
}

static void handleLoadProfile() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  int idx = doc["index"] | 0;
  if (idx >= 0 && idx < (int)profiles.size()) {
    currentProfileIndex = idx;
    storage::saveCurrentProfileIndex(idx);
  }
  S->send(200);
}

static void handleNewProfile() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  Profile p;
  p.name            = doc["name"].as<String>();
  p.targetTemp      = doc["targetTemp"]      | 180.0f;
  p.maxPower        = doc["maxPower"]        | 80.0f;
  p.kp              = doc["kp"]              | 4.0f;
  p.cutTemp         = doc["cutTemp"]         | 195.0f;
  p.heatupPower     = doc["heatupPower"]     | 100.0f;
  p.headsPower      = doc["headsPower"]      | 25.0f;
  p.headsDuration_s = doc["headsDuration_s"] | 900;
  p.tailsTemp       = doc["tailsTemp"]       | (p.targetTemp + 4.0f);
  p.tailsPower      = doc["tailsPower"]      | 35.0f;
  p.valveHeatup     = doc["valveHeatup"]     | 100;
  p.valveHeads      = doc["valveHeads"]      | 30;
  p.valveHearts     = doc["valveHearts"]     | 80;
  p.valveTails      = doc["valveTails"]      | 50;
  profiles.push_back(p);
  storage::saveProfiles();
  S->send(200);
}

static void handleDeleteProfile() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  int idx = doc["index"] | -1;
  if (idx < 0 || idx >= (int)profiles.size() || profiles.size() <= 1) { S->send(400); return; }
  profiles.erase(profiles.begin() + idx);
  if (currentProfileIndex >= (int)profiles.size()) currentProfileIndex = profiles.size() - 1;
  storage::saveProfiles();
  storage::saveCurrentProfileIndex(currentProfileIndex);
  S->send(200);
}

static void handleBatch() {
  if (S->method() == HTTP_POST) {
    if (!S->hasArg("plain")) { S->send(400); return; }
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
    currentBatch.washABV = doc["washABV"] | currentBatch.washABV;
    currentBatch.volume  = doc["volume"]  | currentBatch.volume;
    if (doc.containsKey("unit"))        currentBatch.unit        = doc["unit"].as<String>();
    if (doc.containsKey("ingredients")) currentBatch.ingredients = doc["ingredients"].as<String>();
    if (doc.containsKey("notes"))       currentBatch.notes       = doc["notes"].as<String>();
    storage::saveBatch();
    S->send(200);
  } else {
    StaticJsonDocument<768> doc;
    doc["washABV"]     = currentBatch.washABV;
    doc["volume"]      = currentBatch.volume;
    doc["unit"]        = currentBatch.unit;
    doc["ingredients"] = currentBatch.ingredients;
    doc["notes"]       = currentBatch.notes;
    String out; serializeJson(doc, out);
    S->send(200, "application/json", out);
  }
}

static void handleExport() {
  String csv = "elapsed_s,temp_f,temp_c,power_pct,est_abv,stage\n";
  for (auto& r : sessionReadings) {
    csv += String(r.ts / 1000) + ","
        +  String(cToF(r.temp), 2) + ","
        +  String(r.temp, 2) + ","
        +  String(r.power, 1) + ","
        +  String(r.abv, 1) + ","
        +  stageName((Stage)r.stage) + "\n";
  }
  S->sendHeader("Content-Disposition", "attachment; filename=kegstill_session.csv");
  S->send(200, "text/csv", csv);
}

static void handleWifi() {
  if (S->method() != HTTP_POST) { S->send(405); return; }
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  String ssid = doc["ssid"] | "";
  String pass = doc["pass"] | "";
  if (ssid.length() == 0) { S->send(400); return; }
  storage::saveWifi(ssid, pass);
  S->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
}

static void handleDismissResume() {
  resumePending = false;
  storage::clearRunSnapshot();
  S->send(200);
}

// ---------- valve ----------
// ---------- valve ----------
static const char* valveStateName(valve::State s) {
  switch (s) {
    case valve::ST_UNKNOWN:     return "UNKNOWN";
    case valve::ST_BOOT_HOMING: return "HOMING";
    case valve::ST_IDLE:        return "IDLE";
    case valve::ST_OPENING:     return "OPENING";
    case valve::ST_CLOSING:     return "CLOSING";
    case valve::ST_FAULT:       return "FAULT";
  }
  return "?";
}

static void handleValve() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  if (doc.containsKey("auto")) {
    valveAutoFollowStage = doc["auto"].as<bool>();
    if (valveAutoFollowStage) control::reapplyStageValve();
  }
  if (doc.containsKey("pos")) {
    int pos = doc["pos"] | 0;
    if (pos < 0) pos = 0; if (pos > 100) pos = 100;
    valveAutoFollowStage = false;
    valve::setPosition((uint8_t)pos);
  }
  if (doc.containsKey("openMs") && doc.containsKey("closeMs")) {
    uint32_t openMs  = doc["openMs"]  | 0;
    uint32_t closeMs = doc["closeMs"] | 0;
    valve::setCalibration(openMs, closeMs);
  }
  if (doc.containsKey("cmd")) {
    String cmd = doc["cmd"].as<String>();
    if      (cmd == "rehome")     valve::rehome();
    else if (cmd == "stop")       valve::emergencyStop();
    else if (cmd == "clearFault") valve::clearFault();
    else if (cmd == "open")       { valveAutoFollowStage = false; valve::setPosition(100); }
    else if (cmd == "close")      { valveAutoFollowStage = false; valve::setPosition(0); }
  }
  S->send(200);
}

static void handleValveStatus() {
  auto st = valve::getStatus();
  StaticJsonDocument<384> doc;
  doc["state"]       = valveStateName(st.state);
  doc["stateIdx"]    = (uint8_t)st.state;
  doc["position"]    = round(st.position * 10) / 10.0;
  doc["target"]      = st.target;
  doc["calibrated"]  = st.calibrated;
  doc["openTimeMs"]  = st.openTimeMs;
  doc["closeTimeMs"] = st.closeTimeMs;
  doc["faultReason"] = st.faultReason;
  doc["autoFollow"]  = valveAutoFollowStage;
  String out; serializeJson(doc, out);
  S->send(200, "application/json", out);
}

// ---------- history ----------
static void handleHistoryList() {
  S->send(200, "application/json", history::listJson());
}

static void handleHistoryGet() {
  if (!S->hasArg("id")) { S->send(400); return; }
  uint16_t id = (uint16_t)S->arg("id").toInt();
  String data = history::getRunJson(id);
  if (data.length() == 0) { S->send(404); return; }
  S->send(200, "application/json", data);
}

static void handleHistoryCsv() {
  if (!S->hasArg("id")) { S->send(400); return; }
  uint16_t id = (uint16_t)S->arg("id").toInt();
  String csv = history::getRunCsv(id);
  if (csv.length() == 0) { S->send(404); return; }
  char fn[48];
  snprintf(fn, sizeof(fn), "attachment; filename=kegstill_run_%u.csv", id);
  S->sendHeader("Content-Disposition", fn);
  S->send(200, "text/csv", csv);
}

static void handleHistoryDelete() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  uint16_t id = (uint16_t)(doc["id"] | 0);
  bool ok = history::deleteRun(id);
  S->send(ok ? 200 : 404);
}

// ---------- BLE scan / select ----------
static void handleBleScan() {
  auto devs = ble_scanner::snapshotSeen();
  String target = ble_scanner::getTargetAddress();
  String out = "{\"target\":\"" + target + "\",\"devices\":[";
  uint32_t now = millis();
  for (size_t i = 0; i < devs.size(); i++) {
    if (i) out += ",";
    auto& d = devs[i];
    String esc = d.name; esc.replace("\\","\\\\"); esc.replace("\"","\\\"");
    out += "{\"addr\":\"" + d.addr + "\"";
    out += ",\"name\":\"" + esc + "\"";
    out += ",\"rssi\":" + String(d.rssi);
    out += ",\"hasTemp\":" + String(d.hasTemp ? "true" : "false");
    if (d.hasTemp) out += ",\"tempC\":" + String(d.lastTempC, 2);
    out += ",\"ageMs\":" + String((uint32_t)(now - d.lastSeenMs));
    out += "}";
  }
  out += "]}";
  S->send(200, "application/json", out);
}

static void handleBleSelect() {
  if (!S->hasArg("plain")) { S->send(400); return; }
  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, S->arg("plain")) != DeserializationError::Ok) { S->send(400); return; }
  String addr = doc["addr"] | "";
  ble_scanner::setTargetAddress(addr);
  storage::saveBleTarget(addr);
  S->send(200);
}

static void handleBleClear() {
  ble_scanner::clearSeen();
  S->send(200);
}

void web_handlers::registerRoutes(WebServer& server) {
  S = &server;
  server.on("/",                  handleRoot);
  server.on("/api/status",        HTTP_GET,  handleStatus);
  server.on("/api/start",         HTTP_POST, handleStart);
  server.on("/api/stop",          HTTP_POST, handleStop);
  server.on("/api/estop",         HTTP_POST, handleEStop);
  server.on("/api/reset",         HTTP_POST, handleReset);
  server.on("/api/power",         HTTP_POST, handlePower);
  server.on("/api/automation",    HTTP_POST, handleAutomation);
  server.on("/api/advance",       HTTP_POST, handleAdvanceStage);
  server.on("/api/profiles",      HTTP_GET,  handleProfiles);
  server.on("/api/profile/load",  HTTP_POST, handleLoadProfile);
  server.on("/api/profile/new",   HTTP_POST, handleNewProfile);
  server.on("/api/profile/delete",HTTP_POST, handleDeleteProfile);
  server.on("/api/batch",         HTTP_ANY,  handleBatch);
  server.on("/api/export",        HTTP_GET,  handleExport);
  server.on("/api/wifi",          HTTP_POST, handleWifi);
  server.on("/api/resume/dismiss",HTTP_POST, handleDismissResume);
  server.on("/api/valve",         HTTP_POST, handleValve);
  server.on("/api/valve/status",  HTTP_GET,  handleValveStatus);
  server.on("/api/history",       HTTP_GET,  handleHistoryList);
  server.on("/api/history/get",   HTTP_GET,  handleHistoryGet);
  server.on("/api/history/csv",   HTTP_GET,  handleHistoryCsv);
  server.on("/api/history/delete",HTTP_POST, handleHistoryDelete);
  server.on("/api/ble/scan",      HTTP_GET,  handleBleScan);
  server.on("/api/ble/select",    HTTP_POST, handleBleSelect);
  server.on("/api/ble/clear",     HTTP_POST, handleBleClear);
}
