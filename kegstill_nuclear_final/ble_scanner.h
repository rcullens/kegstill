// ble_scanner.h - CQ60 BLE thermometer scanner
#pragma once
#include <Arduino.h>
#include <vector>

// CQ60 advertised sensor channels.
enum CQ60Channel : uint8_t {
  CQ_CALC_AMBIENT  = 0,   // offset 6-7  - black-end ambient (caps ~85C, NOT for still)
  CQ_CALC_INTERNAL = 1,   // offset 8-9  - min(tip,ring1,ring2) - what the app shows
  CQ_TIP           = 2,   // offset 10-11 - raw tip sensor (DEFAULT for still: deepest in vapor)
  CQ_RING1         = 3,   // offset 12-13
  CQ_RING2         = 4,   // offset 14-15
  CQ_AMBIENT_RAW   = 5    // offset 16-17 - raw black-end ambient
};
const char* cq60ChannelName(uint8_t ch);

struct CQ60Reading {
  bool    valid;
  uint8_t battery;        // %
  float   ch[6];          // °C, indexed by CQ60Channel
};

struct SeenDevice {
  String      addr;
  String      name;
  int         rssi;
  bool        hasTemp;
  bool        isCQ60;
  uint8_t     battery;    // 0 if unknown
  float       ch[6];      // last CQ60 reading per channel (only valid if isCQ60)
  uint32_t    lastSeenMs;
};

namespace ble_scanner {
  void  begin();
  void  poll();
  CQ60Reading parseCQ60(const uint8_t* data, size_t len);

  std::vector<SeenDevice> snapshotSeen();
  void  clearSeen();
  void  setTargetAddress(const String& addr);
  String getTargetAddress();
  void  setSourceChannel(uint8_t ch);   // 0..5 (CQ60Channel)
  uint8_t getSourceChannel();
}
