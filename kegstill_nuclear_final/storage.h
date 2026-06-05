// storage.h - NVS persistence (Preferences)
#pragma once
#include <Arduino.h>
#include "state.h"

namespace storage {
  void begin();

  // WiFi
  void saveWifi(const String& ssid, const String& pass);
  bool loadWifi(String& ssid, String& pass);

  // Profiles (saved as a single JSON blob)
  void saveProfiles();
  bool loadProfiles();
  void saveCurrentProfileIndex(int idx);
  int  loadCurrentProfileIndex();

  // Automation flag
  void saveAutomation(bool en);
  bool loadAutomation();

  // Batch
  void saveBatch();
  bool loadBatch();

  // Run resume metadata
  void saveRunSnapshot();      // call periodically while running
  void clearRunSnapshot();     // call on clean stop / reset / estop ack
  bool loadRunSnapshot();      // sets resume* vars; returns true if a run was active
}
