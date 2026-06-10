// thermocouple.h - MAX31856 K-type thermocouple (SPI)
#pragma once
#include <Arduino.h>

namespace thermocouple {
  void  begin();
  void  poll();                  // call from loop; reads at ~5 Hz
  float getTempC();              // last good reading (°C)
  bool  isValid();               // true if last read was clean
  uint8_t getFault();            // raw MAX31856 fault byte (0 = OK)
  String getFaultStr();
  uint32_t lastUpdateMs();
}
