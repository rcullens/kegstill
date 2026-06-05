// history.h - per-run persistent history on LittleFS
#pragma once
#include <Arduino.h>

namespace history {
  bool begin();                       // mount LittleFS, ensure /runs dir
  uint16_t nextRunId();               // sequence counter (NVS)
  bool saveCurrentRun();              // dump sessionReadings + meta to /runs/run_NNNN.json
  String listJson();                  // index of all runs as JSON array
  String getRunJson(uint16_t id);     // full payload for one run
  String getRunCsv(uint16_t id);      // CSV for download
  bool deleteRun(uint16_t id);
}
