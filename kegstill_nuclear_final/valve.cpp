// valve.cpp - 3-wire smart ball valve.
#include "valve.h"
#include "config.h"
#include "state.h"
#include "storage.h"

using namespace valve;

static State    s_state          = ST_UNKNOWN;
static float    s_position       = 0.0f;     
static uint8_t  s_target         = 0;
static uint32_t s_lastTick       = 0;
static uint32_t s_moveStart      = 0;
static uint32_t s_moveDuration   = 0;        
static unsigned long s_openTimeMs     = 10000;    
static unsigned long s_closeTimeMs    = 10000;
static bool     s_calibrated     = false;
static String   s_faultReason    = "";

static void driveStop() {
  digitalWrite(VALVE_OPEN_PIN,  LOW);
  digitalWrite(VALVE_CLOSE_PIN, LOW);
}
static void driveOpen() {
  digitalWrite(VALVE_CLOSE_PIN, LOW);
  delay(VALVE_REVERSE_DWELL_MS);
  digitalWrite(VALVE_OPEN_PIN,  HIGH);
}
static void driveClose() {
  digitalWrite(VALVE_OPEN_PIN,  LOW);
  delay(VALVE_REVERSE_DWELL_MS);
  digitalWrite(VALVE_CLOSE_PIN, HIGH);
}

void valve::begin() {
  pinMode(VALVE_OPEN_PIN,  OUTPUT); digitalWrite(VALVE_OPEN_PIN,  LOW);
  pinMode(VALVE_CLOSE_PIN, OUTPUT); digitalWrite(VALVE_CLOSE_PIN, LOW);

  storage::loadValveCalib(s_openTimeMs, s_closeTimeMs, s_calibrated);
  Serial.printf("[VALVE] init calibrated=%d open=%lums close=%lums\n",
                s_calibrated, (unsigned long)s_openTimeMs, (unsigned long)s_closeTimeMs);

  s_position = 0; 
  s_state    = ST_IDLE;
  Serial.println("[VALVE] Boot-homing DISABLED for power stability. Home manually via UI.");

  s_lastTick = millis();
  currentValvePos = (uint8_t)s_position;
}

uint8_t valve::getPosition() { return (uint8_t)(s_position + 0.5f); }

void valve::setPosition(uint8_t pct) {
  if (pct > 100) pct = 100;
  s_target = pct;
  float delta = (float)pct - s_position;
  if (fabsf(delta) < VALVE_DEADBAND_PCT) { driveStop(); s_state = ST_IDLE; return; }

  unsigned long travelMs = (delta > 0) ? s_openTimeMs : s_closeTimeMs;
  s_moveDuration    = (uint32_t)(fabsf(delta) * (float)travelMs / 100.0f);
  if (pct == 0)   s_moveDuration = (uint32_t)((float)s_closeTimeMs * VALVE_HOMING_OVERSHOOT);
  if (pct == 100) s_moveDuration = (uint32_t)((float)s_openTimeMs  * VALVE_HOMING_OVERSHOOT);

  s_moveStart = millis();
  if (delta > 0) { s_state = ST_OPENING; driveOpen(); }
  else           { s_state = ST_CLOSING; driveClose(); }
}

void valve::setCalibration(unsigned long openMs, unsigned long closeMs) {
  if (openMs  < 500 || openMs  > 120000) return; 
  if (closeMs < 500 || closeMs > 120000) return;
  s_openTimeMs  = openMs;
  s_closeTimeMs = closeMs;
  s_calibrated  = true;
  storage::saveValveCalib(s_openTimeMs, s_closeTimeMs, true);
  Serial.printf("[VALVE] CAL set: open=%lums close=%lums\n", openMs, closeMs);
}

void valve::rehome() {
  s_target      = 0;
  s_state       = ST_BOOT_HOMING;
  s_moveStart   = millis();
  s_moveDuration = (uint32_t)((float)s_closeTimeMs * VALVE_HOMING_OVERSHOOT);
  driveClose();
  Serial.println("[VALVE] Manual re-homing initiated...");
}

void valve::emergencyStop() { driveStop(); s_state = ST_IDLE; }
void valve::clearFault() { s_state = ST_IDLE; }

valve::Status valve::getStatus() {
  valve::Status st;
  st.state = s_state; st.position = s_position; st.target = s_target;
  st.calibrated = s_calibrated; st.openTimeMs = s_openTimeMs; st.closeTimeMs = s_closeTimeMs;
  return st;
}

void valve::poll() {
  uint32_t now = millis();
  uint32_t dt  = now - s_lastTick;
  s_lastTick = now;

  if (s_state == ST_IDLE) return;

  if (s_state == ST_BOOT_HOMING || s_state == ST_CLOSING) {
    s_position -= (float)dt * 100.0f / (float)s_closeTimeMs;
    if (s_position < 0) s_position = 0;
  } else if (s_state == ST_OPENING) {
    s_position += (float)dt * 100.0f / (float)s_openTimeMs;
    if (s_position > 100) s_position = 100;
  }

  if (now - s_moveStart >= s_moveDuration) {
    driveStop();
    if (s_state == ST_BOOT_HOMING || s_target == 0) s_position = 0;
    if (s_target == 100) s_position = 100;
    s_state = ST_IDLE;
    Serial.println("[VALVE] move complete");
  }
  currentValvePos = (uint8_t)(s_position + 0.5f);
}
