// ble_scanner.h - CQ60 BLE thermometer scanner
#pragma once
#include <Arduino.h>

namespace ble_scanner {
  void begin();        // init NimBLE + start scan
  void poll();         // call from loop (currently no-op; scan is callback-driven)
  float parseCQ60(const uint8_t* data, size_t len);  // exposed for tests
}
