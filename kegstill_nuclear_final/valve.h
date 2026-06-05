// valve.h - motorized ball valve placeholder driver
#pragma once
#include <Arduino.h>

namespace valve {
  void begin();                       // configure pins
  void setPosition(uint8_t pct);      // 0-100, 1% increments
  uint8_t getPosition();              // last commanded position
  void poll();                        // call from loop (drives steps over time)
}
