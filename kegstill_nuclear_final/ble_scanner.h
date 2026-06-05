// ble_scanner.h - CQ60 BLE thermometer scanner
#pragma once
#include <Arduino.h>
#include <vector>

struct SeenDevice {
  String  addr;          // MAC string (lowercase, with colons)
  String  name;
  int     rssi;
  bool    hasTemp;       // true if parser extracted a valid temp from this ad
  float   lastTempC;     // last valid temp parsed (else 0)
  uint32_t lastSeenMs;
};

namespace ble_scanner {
  void  begin();                              // init NimBLE + start scan
  void  poll();                               // call from loop
  float parseCQ60(const uint8_t* data, size_t len);

  std::vector<SeenDevice> snapshotSeen();     // copy of seen-devices table
  void  clearSeen();
  void  setTargetAddress(const String& addr); // "" = auto (any CQ60-named)
  String getTargetAddress();
}
