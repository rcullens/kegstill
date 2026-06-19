// valve.h - 3-wire smart ball valve driver
#pragma once
#include <Arduino.h>

namespace valve {
  enum State : uint8_t {
    ST_UNKNOWN     = 0,   
    ST_BOOT_HOMING = 1,   
    ST_IDLE        = 2,
    ST_OPENING     = 3,
    ST_CLOSING     = 4,
    ST_FAULT       = 5
  };

  struct Status {
    State    state;
    float    position;        
    uint8_t  target;          
    bool     calibrated;
    unsigned long openTimeMs;      
    unsigned long closeTimeMs;     
    String   faultReason;
  };

  void begin();
  void poll();                                    
  void setPosition(uint8_t pct);
  uint8_t getPosition();
  void setCalibration(unsigned long openMs, unsigned long closeMs); 
  void rehome();                                  
  void emergencyStop();
  void clearFault();
  Status getStatus();
}
