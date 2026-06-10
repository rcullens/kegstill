// thermocouple.cpp - MAX31856 K-type driver
#include "thermocouple.h"
#include "config.h"
#include "state.h"
#include <Adafruit_MAX31856.h>

static Adafruit_MAX31856 maxtc = Adafruit_MAX31856(
  TC_CS_PIN, TC_SDI_PIN, TC_SDO_PIN, TC_SCK_PIN);

static float    s_tempC      = 0.0f;
static bool     s_valid      = false;
static uint8_t  s_fault      = 0xFF;
static uint32_t s_lastRead   = 0;
static uint32_t s_lastUpdate = 0;

void thermocouple::begin() {
  if (!maxtc.begin()) {
    Serial.println("[TC] MAX31856 init FAILED (check wiring)");
  } else {
    Serial.println("[TC] MAX31856 init OK");
  }
  maxtc.setThermocoupleType(MAX31856_TCTYPE_K);
  // 60 Hz mains rejection (use _50 if you're in Europe)
  maxtc.setNoiseFilter(MAX31856_NOISE_FILTER_60HZ);
  // continuous conversion - reads will be ready every ~100ms
  maxtc.setConversionMode(MAX31856_CONTINUOUS);
}

void thermocouple::poll() {
  uint32_t now = millis();
  if (now - s_lastRead < 200) return;   // ~5 Hz polling
  s_lastRead = now;

  uint8_t fault = maxtc.readFault();
  float   t     = maxtc.readThermocoupleTemperature();

  s_fault = fault;
  if (fault == 0 && isfinite(t) && t > -50.0f && t < 1300.0f) {
    s_tempC      = t;
    s_valid      = true;
    s_lastUpdate = now;
    currentTempC   = t;
    lastBleUpdate  = now;   // reuse the global timestamp (renamed semantically)
    bleTempValid   = true;
  } else {
    s_valid      = false;
    bleTempValid = false;
  }

  static uint32_t lastLog = 0;
  if (now - lastLog > 2000) {
    lastLog = now;
    if (s_valid) Serial.printf("[TC] %.2fC (%.1fF)  fault=0x00\n", s_tempC, s_tempC * 9.0f/5.0f + 32.0f);
    else         Serial.printf("[TC] INVALID  fault=0x%02X (%s)\n", fault, thermocouple::getFaultStr().c_str());
  }
}

float    thermocouple::getTempC()      { return s_tempC; }
bool     thermocouple::isValid()       { return s_valid; }
uint8_t  thermocouple::getFault()      { return s_fault; }
uint32_t thermocouple::lastUpdateMs()  { return s_lastUpdate; }

String thermocouple::getFaultStr() {
  if (s_fault == 0) return "OK";
  String s;
  if (s_fault & MAX31856_FAULT_CJRANGE) s += "CJ_RANGE ";
  if (s_fault & MAX31856_FAULT_TCRANGE) s += "TC_RANGE ";
  if (s_fault & MAX31856_FAULT_CJHIGH)  s += "CJ_HIGH ";
  if (s_fault & MAX31856_FAULT_CJLOW)   s += "CJ_LOW ";
  if (s_fault & MAX31856_FAULT_TCHIGH)  s += "TC_HIGH ";
  if (s_fault & MAX31856_FAULT_TCLOW)   s += "TC_LOW ";
  if (s_fault & MAX31856_FAULT_OVUV)    s += "OV/UV ";
  if (s_fault & MAX31856_FAULT_OPEN)    s += "OPEN_CIRCUIT ";
  s.trim();
  return s;
}
