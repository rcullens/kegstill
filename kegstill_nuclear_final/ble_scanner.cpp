// ble_scanner.cpp - CQ60 thermometer BLE scanner (NimBLE v2)
#include "ble_scanner.h"
#include "state.h"
#include "config.h"
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <algorithm>

static NimBLEScan*       pBLEScan  = nullptr;
static SemaphoreHandle_t seenMutex = nullptr;
static const size_t      MAX_SEEN  = 24;
static const uint32_t    PRUNE_MS  = 60000;  // drop entries unseen for 60s

static std::vector<SeenDevice> g_seen;
static String                  g_target = "";   // "" = auto (any CQ60-named)

// ---- temp parsing (unchanged from before) ----
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

static void upsertSeen(const String& addr, const String& name, int rssi,
                       bool hasTemp, float tempC) {
  if (!seenMutex) return;
  xSemaphoreTake(seenMutex, portMAX_DELAY);
  bool found = false;
  for (auto& d : g_seen) {
    if (d.addr == addr) {
      d.name       = name;
      d.rssi       = rssi;
      if (hasTemp) { d.hasTemp = true; d.lastTempC = tempC; }
      d.lastSeenMs = millis();
      found = true;
      break;
    }
  }
  if (!found) {
    if (g_seen.size() >= MAX_SEEN) {
      // evict the oldest entry
      size_t oldest = 0;
      for (size_t i = 1; i < g_seen.size(); i++)
        if (g_seen[i].lastSeenMs < g_seen[oldest].lastSeenMs) oldest = i;
      g_seen.erase(g_seen.begin() + oldest);
    }
    SeenDevice d;
    d.addr = addr; d.name = name; d.rssi = rssi;
    d.hasTemp = hasTemp; d.lastTempC = hasTemp ? tempC : 0.0f;
    d.lastSeenMs = millis();
    g_seen.push_back(d);
  }
  xSemaphoreGive(seenMutex);
}

static void pruneStale() {
  if (!seenMutex) return;
  xSemaphoreTake(seenMutex, portMAX_DELAY);
  uint32_t now = millis();
  g_seen.erase(std::remove_if(g_seen.begin(), g_seen.end(),
    [&](const SeenDevice& d){ return (now - d.lastSeenMs) > PRUNE_MS; }),
    g_seen.end());
  xSemaphoreGive(seenMutex);
}

class CQ60ScanCallbacks : public NimBLEScanCallbacks {
public:
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    if (!dev) return;
    String addr = String(dev->getAddress().toString().c_str());
    addr.toLowerCase();
    String name = dev->haveName() ? String(dev->getName().c_str()) : String("(unnamed)");
    int    rssi = dev->getRSSI();

    // try to parse temperature regardless — we record hasTemp for the UI list
    float tempC = -999.0f;
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      tempC = ble_scanner::parseCQ60(
        reinterpret_cast<const uint8_t*>(md.data()), md.length());
    }
    bool gotTemp = (tempC > 0.0f);

    upsertSeen(addr, name, rssi, gotTemp, tempC);

    // decide whether THIS device is our active probe
    bool isTarget;
    if (g_target.length() > 0) {
      isTarget = (addr == g_target);
    } else {
      isTarget = (name.indexOf("CQ60") >= 0);
    }
    if (isTarget && gotTemp) {
      currentTempC  = tempC;
      lastBleUpdate = millis();
      bleTempValid  = true;
    }
  }
};

void ble_scanner::begin() {
  seenMutex = xSemaphoreCreateMutex();
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
  static uint32_t lastPrune = 0;
  if (millis() - lastPrune > 5000) { lastPrune = millis(); pruneStale(); }
}

std::vector<SeenDevice> ble_scanner::snapshotSeen() {
  std::vector<SeenDevice> out;
  if (!seenMutex) return out;
  xSemaphoreTake(seenMutex, portMAX_DELAY);
  out = g_seen;
  xSemaphoreGive(seenMutex);
  return out;
}

void ble_scanner::clearSeen() {
  if (!seenMutex) return;
  xSemaphoreTake(seenMutex, portMAX_DELAY);
  g_seen.clear();
  xSemaphoreGive(seenMutex);
}

void ble_scanner::setTargetAddress(const String& addr) {
  String a = addr; a.toLowerCase();
  g_target = a;
  // invalidate current reading so the dashboard goes blank until the new
  // target advertises a fresh temp
  bleTempValid = false;
  Serial.printf("[BLE] target set to '%s'\n", g_target.length() ? g_target.c_str() : "(auto)");
}

String ble_scanner::getTargetAddress() { return g_target; }
