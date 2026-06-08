// valve.h - motorized ball valve driver (2-wire DC + limit switches, timed positioning)
#pragma once
#include <Arduino.h>

namespace valve {
  enum State : uint8_t {
    ST_UNKNOWN     = 0,   // boot, never homed
    ST_BOOT_HOMING = 1,   // driving closed to find home
    ST_IDLE        = 2,
    ST_OPENING     = 3,
    ST_CLOSING     = 4,
    ST_CAL_OPENING = 5,
    ST_CAL_CLOSING = 6,
    ST_FAULT       = 7
  };

  struct Status {
    State    state;
    float    position;          // 0-100, best estimate
    uint8_t  target;            // 0-100
    bool     calibrated;
    bool     atOpenLimit;
    bool     atClosedLimit;
    uint32_t openTimeMs;        // calibrated full-open travel
    uint32_t closeTimeMs;       // calibrated full-close travel
    String   faultReason;
  };

  void begin();
  void poll();                  // call every loop
  void setPosition(uint8_t pct);
  uint8_t getPosition();        // returns rounded current position
  void startCalibration();      // kicks off close->open->close sequence
  void emergencyStop();
  void clearFault();
  Status getStatus();
}
