// storage.cpp - NVS persistence (Preferences)
#include "storage.h"
#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences prefs;
static const char* NS = "kegstill";

void storage::begin() {
  // each call opens the namespace; we open on demand instead
}

// ---------- WiFi ----------
void storage::saveWifi(const String& ssid, const String& pass) {
  prefs.begin(NS, false);
  prefs.putString("wifi_ssid", ssid);
  prefs.putString("wifi_pass", pass);
  prefs.end();
}

bool storage::loadWifi(String& ssid, String& pass) {
  prefs.begin(NS, true);
  ssid = prefs.getString("wifi_ssid", "");
  pass = prefs.getString("wifi_pass", "");
  prefs.end();
  return ssid.length() > 0;
}

// ---------- Profiles ----------
void storage::saveProfiles() {
  StaticJsonDocument<3072> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto& p : profiles) {
    JsonObject o = arr.createNestedObject();
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
  String json; serializeJson(doc, json);
  prefs.begin(NS, false);
  prefs.putString("profiles_json", json);
  prefs.end();
}

bool storage::loadProfiles() {
  prefs.begin(NS, true);
  String json = prefs.getString("profiles_json", "");
  prefs.end();
  if (json.length() == 0) return false;

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

  profiles.clear();
  for (JsonObject o : doc.as<JsonArray>()) {
    Profile p;
    p.name            = o["name"].as<String>();
    p.targetTemp      = o["targetTemp"]      | 180.0f;
    p.maxPower        = o["maxPower"]        | 80.0f;
    p.kp              = o["kp"]              | 4.0f;
    p.cutTemp         = o["cutTemp"]         | 195.0f;
    p.heatupPower     = o["heatupPower"]     | 100.0f;
    p.headsPower      = o["headsPower"]      | 25.0f;
    p.headsDuration_s = o["headsDuration_s"] | 900;     // 15 min
    p.tailsTemp       = o["tailsTemp"]       | (p.targetTemp + 4.0f);
    p.tailsPower      = o["tailsPower"]      | 35.0f;
    p.valveHeatup     = o["valveHeatup"]     | 100;
    p.valveHeads      = o["valveHeads"]      | 30;
    p.valveHearts     = o["valveHearts"]     | 80;
    p.valveTails      = o["valveTails"]      | 50;
    profiles.push_back(p);
  }
  return !profiles.empty();
}

void storage::saveCurrentProfileIndex(int idx) {
  prefs.begin(NS, false);
  prefs.putInt("cur_prof", idx);
  prefs.end();
}

int storage::loadCurrentProfileIndex() {
  prefs.begin(NS, true);
  int idx = prefs.getInt("cur_prof", 0);
  prefs.end();
  return idx;
}

// ---------- Automation ----------
void storage::saveAutomation(bool en) {
  prefs.begin(NS, false);
  prefs.putBool("auto_en", en);
  prefs.end();
}

bool storage::loadAutomation() {
  prefs.begin(NS, true);
  bool v = prefs.getBool("auto_en", false);
  prefs.end();
  return v;
}

// ---------- Batch ----------
void storage::saveBatch() {
  prefs.begin(NS, false);
  prefs.putFloat ("b_abv",   currentBatch.washABV);
  prefs.putFloat ("b_vol",   currentBatch.volume);
  prefs.putString("b_unit",  currentBatch.unit);
  prefs.putString("b_ing",   currentBatch.ingredients);
  prefs.putString("b_notes", currentBatch.notes);
  prefs.end();
}

bool storage::loadBatch() {
  prefs.begin(NS, true);
  currentBatch.washABV     = prefs.getFloat ("b_abv",   currentBatch.washABV);
  currentBatch.volume      = prefs.getFloat ("b_vol",   currentBatch.volume);
  currentBatch.unit        = prefs.getString("b_unit",  currentBatch.unit);
  currentBatch.ingredients = prefs.getString("b_ing",   currentBatch.ingredients);
  currentBatch.notes       = prefs.getString("b_notes", currentBatch.notes);
  prefs.end();
  return true;
}

// ---------- Run snapshot ----------
void storage::saveRunSnapshot() {
  unsigned long elapsedSec = isRunning ? ((millis() - startMillis) / 1000UL) + (resumeOffsetMs / 1000UL) : 0;
  prefs.begin(NS, false);
  prefs.putBool ("run_active",  isRunning && !estopActive);
  prefs.putULong("run_elapsed", elapsedSec);
  prefs.putUChar("run_stage",   (uint8_t)currentStage);
  prefs.putInt  ("run_prof",    currentProfileIndex);
  prefs.end();
}

void storage::clearRunSnapshot() {
  prefs.begin(NS, false);
  prefs.putBool ("run_active",  false);
  prefs.putULong("run_elapsed", 0);
  prefs.putUChar("run_stage",   (uint8_t)STAGE_IDLE);
  prefs.end();
}

bool storage::loadRunSnapshot() {
  prefs.begin(NS, true);
  bool          active = prefs.getBool ("run_active",  false);
  unsigned long sec    = prefs.getULong("run_elapsed", 0);
  uint8_t       stg    = prefs.getUChar("run_stage",   STAGE_IDLE);
  int           pidx   = prefs.getInt  ("run_prof",    0);
  prefs.end();
  if (active && sec > 0) {
    resumePending     = true;
    resumeElapsedSec  = sec;
    resumeStage       = stg;
    resumeProfileIdx  = pidx;
    return true;
  }
  return false;
}

// ---------- BLE target ----------
void storage::saveBleTarget(const String& addr) {
  prefs.begin(NS, false);
  prefs.putString("ble_target", addr);
  prefs.end();
}

String storage::loadBleTarget() {
  prefs.begin(NS, true);
  String a = prefs.getString("ble_target", "");
  prefs.end();
  return a;
}

// ---------- Valve calibration ----------
void storage::saveValveCalib(uint32_t openMs, uint32_t closeMs, bool calibrated) {
  prefs.begin(NS, false);
  prefs.putULong("v_open_ms",  openMs);
  prefs.putULong("v_close_ms", closeMs);
  prefs.putBool ("v_cal",      calibrated);
  prefs.end();
}

void storage::loadValveCalib(uint32_t& openMs, uint32_t& closeMs, bool& calibrated) {
  prefs.begin(NS, true);
  openMs     = prefs.getULong("v_open_ms",  openMs);
  closeMs    = prefs.getULong("v_close_ms", closeMs);
  calibrated = prefs.getBool ("v_cal",      false);
  prefs.end();
}
