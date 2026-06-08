// valve.h - 3-wire smart ball valve driver (internal limit switches, timed positioning)
#pragma once
#include <Arduino.h>

namespace valve {
  enum State : uint8_t {
    ST_UNKNOWN     = 0,   // boot, never homed
    ST_BOOT_HOMING = 1,   // driving closed to find home (timed)
    ST_IDLE        = 2,
    ST_OPENING     = 3,
    ST_CLOSING     = 4,
    ST_FAULT       = 5
  };

  struct Status {
    State    state;
    float    position;        // 0-100, best estimate
    uint8_t  target;          // 0-100
    bool     calibrated;
    uint32_t openTimeMs;      // full-open travel duration (manually entered)
    uint32_t closeTimeMs;     // full-close travel duration
    String   faultReason;
  };

  void begin();
  void poll();                                    // call every loop
  void setPosition(uint8_t pct);
  uint8_t getPosition();
  void setCalibration(uint32_t openMs, uint32_t closeMs); // user-entered times
  void rehome();                                  // re-drive to closed (re-zero)
  void emergencyStop();
  void clearFault();
  Status getStatus();
}
