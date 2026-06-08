// valve.cpp - 3-wire smart ball valve. ESP32 drives two relays/MOSFETs that
// energize the OPEN or CLOSE line. The valve has its own internal limit
// switches that cut the motor at end of travel, so we never overdrive.
// Position is tracked purely by timing.

#include "valve.h"
#include "config.h"
#include "state.h"
#include "storage.h"

using namespace valve;

// ---------- internal state ----------
static State    s_state          = ST_UNKNOWN;
static float    s_position       = 0.0f;     // 0-100 best estimate
static uint8_t  s_target         = 0;
static uint32_t s_lastTick       = 0;
static uint32_t s_moveStart      = 0;
static uint32_t s_moveDuration   = 0;        // commanded run-time for this move
static uint32_t s_openTimeMs     = 10000;    // default 10s until user calibrates
static uint32_t s_closeTimeMs    = 10000;
static bool     s_calibrated     = false;
static String   s_faultReason    = "";

// ---------- low-level drive ----------
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

static void enterFault(const char* why) {
  driveStop();
  s_state = ST_FAULT;
  s_faultReason = String(why);
  Serial.printf("[VALVE] FAULT: %s\n", why);
}

// ---------- API ----------
void valve::begin() {
  pinMode(VALVE_OPEN_PIN,  OUTPUT); digitalWrite(VALVE_OPEN_PIN,  LOW);
  pinMode(VALVE_CLOSE_PIN, OUTPUT); digitalWrite(VALVE_CLOSE_PIN, LOW);

  storage::loadValveCalib(s_openTimeMs, s_closeTimeMs, s_calibrated);
  Serial.printf("[VALVE] init calibrated=%d open=%lums close=%lums\n",
                s_calibrated, (unsigned long)s_openTimeMs, (unsigned long)s_closeTimeMs);

  // Boot home: drive close for the full duration + overshoot. Internal limit
  // switch in the valve will cut its motor when it bottoms out — the extra
  // time is harmless.
  s_position    = 100;   // pessimistic: assume it might be open
  s_state       = ST_BOOT_HOMING;
  s_moveStart   = millis();
  s_moveDuration = (uint32_t)((float)s_closeTimeMs * VALVE_HOMING_OVERSHOOT);
  driveClose();
  Serial.printf("[VALVE] homing closed for %lums...\n", (unsigned long)s_moveDuration);

  s_lastTick = millis();
  currentValvePos = (uint8_t)s_position;
}

uint8_t valve::getPosition() {
  if (s_position < 0)   return 0;
  if (s_position > 100) return 100;
  return (uint8_t)(s_position + 0.5f);
}

void valve::setPosition(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (s_state == ST_FAULT) {
    Serial.println("[VALVE] setPosition ignored: FAULT (clear it first)");
    return;
  }
  if (s_state == ST_BOOT_HOMING) {
    s_target = pct;   // queue until home completes
    return;
  }

  s_target = pct;
  float delta = (float)pct - s_position;
  if (fabsf(delta) < VALVE_DEADBAND_PCT) { driveStop(); s_state = ST_IDLE; return; }

  // Compute commanded run-time from delta + per-direction calibration.
  uint32_t travelMs = (delta > 0) ? s_openTimeMs : s_closeTimeMs;
  s_moveDuration    = (uint32_t)(fabsf(delta) * (float)travelMs / 100.0f);
  // For target=0 or 100 we drive a bit longer so internal limit absorbs drift.
  if (pct == 0)   s_moveDuration = (uint32_t)((float)s_closeTimeMs * VALVE_HOMING_OVERSHOOT);
  if (pct == 100) s_moveDuration = (uint32_t)((float)s_openTimeMs  * VALVE_HOMING_OVERSHOOT);

  s_moveStart = millis();
  if (delta > 0) { s_state = ST_OPENING; driveOpen(); }
  else           { s_state = ST_CLOSING; driveClose(); }
  Serial.printf("[VALVE] target=%u%% from %.1f%% (%s for %lums)\n",
                pct, s_position, delta > 0 ? "OPEN" : "CLOSE",
                (unsigned long)s_moveDuration);
}

void valve::setCalibration(uint32_t openMs, uint32_t closeMs) {
  if (openMs  < 500 || openMs  > 120000) { Serial.println("[VALVE] cal openMs out of range");  return; }
  if (closeMs < 500 || closeMs > 120000) { Serial.println("[VALVE] cal closeMs out of range"); return; }
  s_openTimeMs  = openMs;
  s_closeTimeMs = closeMs;
  s_calibrated  = true;
  storage::saveValveCalib(s_openTimeMs, s_closeTimeMs, true);
  Serial.printf("[VALVE] CAL set: open=%lums close=%lums\n",
                (unsigned long)openMs, (unsigned long)closeMs);
}

void valve::rehome() {
  if (s_state == ST_FAULT) return;
  s_target      = 0;
  s_state       = ST_BOOT_HOMING;
  s_moveStart   = millis();
  s_moveDuration = (uint32_t)((float)s_closeTimeMs * VALVE_HOMING_OVERSHOOT);
  driveClose();
  Serial.println("[VALVE] re-homing...");
}

void valve::emergencyStop() {
  driveStop();
  s_state  = ST_IDLE;
  s_target = (uint8_t)(s_position + 0.5f);
}

void valve::clearFault() {
  if (s_state != ST_FAULT) return;
  s_faultReason = "";
  s_state = ST_IDLE;
}

Status valve::getStatus() {
  Status st;
  st.state        = s_state;
  st.position     = s_position;
  st.target       = s_target;
  st.calibrated   = s_calibrated;
  st.openTimeMs   = s_openTimeMs;
  st.closeTimeMs  = s_closeTimeMs;
  st.faultReason  = s_faultReason;
  return st;
}

// ---------- poll ----------
void valve::poll() {
  uint32_t now = millis();
  uint32_t dt  = now - s_lastTick;
  s_lastTick = now;

  switch (s_state) {
    case ST_UNKNOWN:
    case ST_IDLE:
    case ST_FAULT:
      break;

    case ST_BOOT_HOMING:
      // We don't have feedback; just drive close for the homing duration.
      if (now - s_moveStart >= s_moveDuration) {
        driveStop();
        s_position = 0;
        s_state = ST_IDLE;
        Serial.println("[VALVE] home complete (position assumed 0)");
        // if a setPosition was queued during homing, execute it now
        if (s_target != 0) {
          uint8_t q = s_target;
          s_target = 0;        // clear so setPosition sees a real delta
          setPosition(q);
        }
      }
      break;

    case ST_OPENING: {
      s_position += (float)dt * 100.0f / (float)s_openTimeMs;
      if (s_position > 100) s_position = 100;
      if (now - s_moveStart >= s_moveDuration) {
        driveStop();
        s_state = ST_IDLE;
        // for target=100 we may have overdriven slightly — snap and trust internal limit
        if (s_target == 100) s_position = 100;
      }
      break;
    }

    case ST_CLOSING: {
      s_position -= (float)dt * 100.0f / (float)s_closeTimeMs;
      if (s_position < 0) s_position = 0;
      if (now - s_moveStart >= s_moveDuration) {
        driveStop();
        s_state = ST_IDLE;
        if (s_target == 0) s_position = 0;
      }
      break;
    }
  }

  if (s_position < 0)   s_position = 0;
  if (s_position > 100) s_position = 100;
  currentValvePos = (uint8_t)(s_position + 0.5f);
}
