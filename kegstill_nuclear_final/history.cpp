// history.cpp - per-run persistent history on LittleFS
#include "history.h"
#include "config.h"
#include "state.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

static const char* RUNS_DIR = "/runs";
static Preferences hprefs;
static const char* NS = "kegstill";

static String runPath(uint16_t id) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/run_%04u.json", RUNS_DIR, id);
  return String(buf);
}

bool history::begin() {
  if (!LittleFS.begin(true)) {     // format on first run
    Serial.println("[FS] LittleFS mount failed");
    return false;
  }
  if (!LittleFS.exists(RUNS_DIR)) LittleFS.mkdir(RUNS_DIR);
  Serial.printf("[FS] LittleFS ok, %u bytes used / %u total\n",
                (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  return true;
}

uint16_t history::nextRunId() {
  hprefs.begin(NS, false);
  uint16_t id = hprefs.getUShort("run_seq", 0) + 1;
  hprefs.putUShort("run_seq", id);
  hprefs.end();
  return id;
}

bool history::saveCurrentRun() {
  if (sessionReadings.empty()) {
    Serial.println("[HIST] no readings, skipping save");
    return false;
  }
  uint16_t id = nextRunId();
  String path = runPath(id);
  File f = LittleFS.open(path, "w");
  if (!f) { Serial.printf("[HIST] open %s failed\n", path.c_str()); return false; }

  // Write streaming JSON manually to avoid huge ArduinoJson buffer
  f.print("{\"id\":"); f.print(id);
  if (currentProfileIndex >= 0 && currentProfileIndex < (int)profiles.size())
    { f.print(",\"profile\":\""); f.print(profiles[currentProfileIndex].name); f.print("\""); }
  else { f.print(",\"profile\":\"?\""); }

  unsigned long elapsed = sessionReadings.empty() ? 0 : sessionReadings.back().ts / 1000UL;
  f.print(",\"duration\":"); f.print(elapsed);
  f.print(",\"points\":"); f.print(sessionReadings.size());
  f.print(",\"batch\":{");
  f.print("\"washABV\":");     f.print(currentBatch.washABV, 2);
  f.print(",\"volume\":");     f.print(currentBatch.volume, 2);
  f.print(",\"unit\":\"");     f.print(currentBatch.unit); f.print("\"");
  f.print(",\"ingredients\":"); {
    String esc = currentBatch.ingredients; esc.replace("\\","\\\\"); esc.replace("\"","\\\"");
    f.print("\""); f.print(esc); f.print("\"");
  }
  f.print(",\"notes\":"); {
    String esc = currentBatch.notes; esc.replace("\\","\\\\"); esc.replace("\"","\\\"");
    f.print("\""); f.print(esc); f.print("\"");
  }
  f.print("}");

  f.print(",\"readings\":[");
  for (size_t i = 0; i < sessionReadings.size(); i++) {
    Reading& r = sessionReadings[i];
    if (i) f.print(",");
    f.print("[");
    f.print(r.ts / 1000UL); f.print(",");
    f.print(r.temp, 2);     f.print(",");
    f.print(r.power, 1);    f.print(",");
    f.print(r.abv, 1);      f.print(",");
    f.print((unsigned)r.stage); f.print(",");
    f.print((unsigned)r.valve);
    f.print("]");
  }
  f.print("]}");
  f.close();
  Serial.printf("[HIST] saved %s (%u pts)\n", path.c_str(), (unsigned)sessionReadings.size());
  return true;
}

String history::listJson() {
  String out = "[";
  bool first = true;
  File root = LittleFS.open(RUNS_DIR);
  if (!root || !root.isDirectory()) { out += "]"; return out; }

  File f = root.openNextFile();
  while (f) {
    if (!f.isDirectory()) {
      String name = f.name();
      // sequence id from filename run_NNNN.json
      int us = name.indexOf("run_");
      int dot = name.indexOf(".json");
      uint16_t id = 0;
      if (us >= 0 && dot > us) id = (uint16_t)name.substring(us + 4, dot).toInt();

      // peek metadata cheaply: read first 256 bytes and grab id/profile/duration
      char buf[256] = {0};
      size_t n = f.read((uint8_t*)buf, sizeof(buf) - 1);
      buf[n] = 0;
      String head = String(buf);

      String prof = "?", dur = "0", pts = "0";
      int p1 = head.indexOf("\"profile\":\""); if (p1 >= 0) { int p2 = head.indexOf("\"", p1 + 11); if (p2>p1) prof = head.substring(p1 + 11, p2); }
      int d1 = head.indexOf("\"duration\":"); if (d1 >= 0) { int d2 = head.indexOf(",", d1); if (d2>d1) dur = head.substring(d1 + 11, d2); }
      int n1 = head.indexOf("\"points\":");   if (n1 >= 0) { int n2 = head.indexOf(",", n1); if (n2>n1) pts = head.substring(n1 + 9,  n2); }

      if (!first) out += ",";
      first = false;
      out += "{\"id\":" + String(id);
      out += ",\"profile\":\"" + prof + "\"";
      out += ",\"duration\":" + dur;
      out += ",\"points\":" + pts;
      out += ",\"bytes\":" + String((unsigned)f.size()) + "}";
    }
    f = root.openNextFile();
  }
  out += "]";
  return out;
}

String history::getRunJson(uint16_t id) {
  String path = runPath(id);
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  String data;
  data.reserve(f.size() + 1);
  while (f.available()) data += (char)f.read();
  f.close();
  return data;
}

String history::getRunCsv(uint16_t id) {
  String json = getRunJson(id);
  if (json.length() == 0) return "";
  // Build CSV from readings array — parse the [[...],[...]] tail
  String csv = "elapsed_s,temp_f,temp_c,power_pct,est_abv,stage,valve_pct\n";
  int idx = json.indexOf("\"readings\":[");
  if (idx < 0) return csv;
  idx += 12;  // past [
  while (idx < (int)json.length()) {
    int lb = json.indexOf('[', idx);
    if (lb < 0) break;
    int rb = json.indexOf(']', lb);
    if (rb < 0) break;
    String row = json.substring(lb + 1, rb);
    // row is e.g.  "12,78.5,40.0,90.5,3,75"
    // we need: elapsed_s,temp_f,temp_c,power_pct,est_abv,stage,valve_pct
    // file stores: ts_s,temp_c,power,abv,stage,valve
    // -> insert temp_f at position 1
    int c0 = row.indexOf(','); if (c0<0) { idx=rb+1; continue; }
    int c1 = row.indexOf(',', c0+1); if (c1<0) { idx=rb+1; continue; }
    String tsS  = row.substring(0, c0);
    String tcS  = row.substring(c0+1, c1);
    String rest = row.substring(c1+1);
    float tc = tcS.toFloat();
    float tf = tc * 9.0f/5.0f + 32.0f;
    csv += tsS + "," + String(tf, 2) + "," + tcS + "," + rest + "\n";
    idx = rb + 1;
    if (json.charAt(idx) != ',') break;
  }
  return csv;
}

bool history::deleteRun(uint16_t id) {
  return LittleFS.remove(runPath(id));
}
