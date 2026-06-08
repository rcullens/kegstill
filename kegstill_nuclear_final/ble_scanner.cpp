// ble_scanner.cpp - CQ60 thermometer BLE scanner with byte-exact packet parser.
//
// Packet layout (after 0xCD 0x05 manufacturer ID):
//   off 0-1   cd 05            manufacturer id
//   off 2-3   01 40            device sub-type
//   off 4     1B               battery %
//   off 5     1B               ring 3 (integer °C, low-res)
//   off 6-7   2B LE * 0.1      calc ambient (black-end, caps ~85C)
//   off 8-9   2B LE * 0.1      calc internal (min of tip,ring1,ring2)
//   off 10-11 2B LE * 0.1      tip raw
//   off 12-13 2B LE * 0.1      ring 1 raw
//   off 14-15 2B LE * 0.1      ring 2 raw
//   off 16-17 2B LE * 0.1      ambient raw
//   off 18-19 2B               humidity/conductivity (unreliable, ignored)
// Reverse-engineered by willemcvu/Ernst79:
// https://github.com/custom-components/ble_monitor/issues/1279

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
static const uint32_t    PRUNE_MS  = 60000;

static std::vector<SeenDevice> g_seen;
static String                  g_target  = "";
static uint8_t                 g_srcCh   = CQ_TIP;   // default for still vapor measurement

const char* cq60ChannelName(uint8_t ch) {
  switch (ch) {
    case CQ_CALC_AMBIENT:  return "Calc Ambient";
    case CQ_CALC_INTERNAL: return "Calc Internal (app)";
    case CQ_TIP:           return "Tip (vapor)";
    case CQ_RING1:         return "Ring 1";
    case CQ_RING2:         return "Ring 2";
    case CQ_AMBIENT_RAW:   return "Ambient Raw";
    default:               return "?";
  }
}

// --- parser ---
CQ60Reading ble_scanner::parseCQ60(const uint8_t* data, size_t len) {
  CQ60Reading r = {};
  if (len < 18) return r;                       // need at least through ambient-raw
  if (data[0] != 0xCD || data[1] != 0x05) return r;

  r.battery = data[4];
  for (uint8_t i = 0; i < 6; i++) {
    size_t off = 6 + i * 2;
    uint16_t raw = (uint16_t)data[off] | ((uint16_t)data[off + 1] << 8);
    r.ch[i] = (float)raw / 10.0f;
  }
  // sanity: battery 0-100, at least one channel in plausible temp range
  if (r.battery > 100) return r;
  bool anyPlausible = false;
  for (int i = 0; i < 6; i++) if (r.ch[i] > -20.0f && r.ch[i] < 400.0f) { anyPlausible = true; break; }
  if (!anyPlausible) return r;
  r.valid = true;
  return r;
}

// --- seen-devices table ---
static void upsertSeen(const String& addr, const String& name, int rssi,
                       const CQ60Reading& cq) {
  if (!seenMutex) return;
  xSemaphoreTake(seenMutex, portMAX_DELAY);
  SeenDevice* slot = nullptr;
  for (auto& d : g_seen) if (d.addr == addr) { slot = &d; break; }
  if (!slot) {
    if (g_seen.size() >= MAX_SEEN) {
      size_t oldest = 0;
      for (size_t i = 1; i < g_seen.size(); i++)
        if (g_seen[i].lastSeenMs < g_seen[oldest].lastSeenMs) oldest = i;
      g_seen.erase(g_seen.begin() + oldest);
    }
    g_seen.push_back({});
    slot = &g_seen.back();
    slot->addr = addr;
  }
  slot->name       = name;
  slot->rssi       = rssi;
  slot->lastSeenMs = millis();
  if (cq.valid) {
    slot->isCQ60  = true;
    slot->hasTemp = true;
    slot->battery = cq.battery;
    for (int i = 0; i < 6; i++) slot->ch[i] = cq.ch[i];
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
    String addr = String(dev->getAddress().toString().c_str()); addr.toLowerCase();
    String name = dev->haveName() ? String(dev->getName().c_str()) : String("(unnamed)");
    int    rssi = dev->getRSSI();

    CQ60Reading cq = {};
    if (dev->haveManufacturerData()) {
      std::string md = dev->getManufacturerData();
      cq = ble_scanner::parseCQ60(
        reinterpret_cast<const uint8_t*>(md.data()), md.length());
    }
    upsertSeen(addr, name, rssi, cq);

    // is this our active probe?
    bool isTarget;
    if (g_target.length() > 0) isTarget = (addr == g_target);
    else                       isTarget = (name.indexOf("CQ60") >= 0);

    if (isTarget && cq.valid) {
      uint8_t ch = g_srcCh < 6 ? g_srcCh : CQ_TIP;
      float t = cq.ch[ch];
      if (t > -20.0f && t < 400.0f) {
        currentTempC  = t;
        bleBattery    = cq.battery;
        lastBleUpdate = millis();
        bleTempValid  = true;
      }
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
  // 100% duty scan — interval == window. CQ60 advertises every ~500ms, but
  // this ensures we never miss one and minimizes time-to-first-reading.
  pBLEScan->setInterval(160);    // 100 ms
  pBLEScan->setWindow(160);      // 100 ms (100% duty)
  pBLEScan->start(0, false);
  Serial.println("[BLE] scan started (100% duty, CQ60 byte-exact parser)");
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
  bleTempValid = false;   // force dashboard blank until new target advertises
  Serial.printf("[BLE] target='%s'\n", g_target.length() ? g_target.c_str() : "(auto)");
}

String ble_scanner::getTargetAddress() { return g_target; }

void ble_scanner::setSourceChannel(uint8_t ch) {
  if (ch > 5) ch = CQ_TIP;
  g_srcCh = ch;
  bleTempValid = false;   // re-confirm with next ad on the new channel
  Serial.printf("[BLE] source channel = %u (%s)\n", g_srcCh, cq60ChannelName(g_srcCh));
}

uint8_t ble_scanner::getSourceChannel() { return g_srcCh; }
