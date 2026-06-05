// ble_scanner.cpp - CQ60 thermometer BLE scanner (NimBLE v2)
#include "ble_scanner.h"
#include "state.h"
#include "config.h"
#include <NimBLEDevice.h>

static NimBLEScan* pBLEScan = nullptr;

// CQ60 advertises manufacturer data containing a marker 0xCD 0x05, with a
// little-endian uint16 temperature (deci-°C) at one of several offsets after
// the marker. We scan a small window of plausible offsets and accept the
// first value that lies in a physical range.
float ble_scanner::parseCQ60(const uint8_t* data, size_t len) {
  if (len < 12) return -999.0f;
  for (size_t i = 0; i + 8 < len; i++) {
    if (data[i] == 0xCD && data[i + 1] == 0x05) {
      for (int off = 6; off < 14; off++) {
        if (i + off + 1 < len) {
          uint16_t raw = (uint16_t)data[i + off] | ((uint16_t)data[i + off + 1] << 8);
          float t = raw / 10.0f;
          if (t > 5.0f && t < 130.0f) return t;
        }
      }
    }
  }
  return -999.0f;
}

class CQ60ScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    if (!dev) return;
    bool nameMatch = dev->getName().find("CQ60") != std::string::npos;
    if (!nameMatch && !dev->haveManufacturerData()) return;

    std::string md = dev->getManufacturerData();
    float t = ble_scanner::parseCQ60(
      reinterpret_cast<const uint8_t*>(md.data()), md.length());
    if (t > 0.0f) {
      currentTempC  = t;
      lastBleUpdate = millis();
      bleTempValid  = true;
    }
  }
};

void ble_scanner::begin() {
  NimBLEDevice::init("KegStill-CQ60");
  pBLEScan = NimBLEDevice::getScan();
  static CQ60ScanCallbacks cb;
  pBLEScan->setScanCallbacks(&cb);
  pBLEScan->setActiveScan(false);
  pBLEScan->setInterval(80);
  pBLEScan->setWindow(40);
  pBLEScan->start(0, false);   // continuous
  Serial.println("[BLE] Scan started");
}

void ble_scanner::poll() {
  // scan is callback-driven; nothing to do
}
