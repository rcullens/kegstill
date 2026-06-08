// valve.cpp - 2-wire DC motorized ball valve with end limit switches.
// Position 0..100% estimated by timing against calibrated full-travel duration.

#include "valve.h"
#include "config.h"
#include "state.h"
#include "storage.h"

using namespace valve;

// ---------- internal state ----------
static State    s_state          = ST_UNKNOWN;
static float    s_position       = 0.0f;     // best estimate, 0-100
static uint8_t  s_target         = 0;
static uint32_t s_lastTick       = 0;
static uint32_t s_moveStart      = 0;        // millis() when current motion began
static uint32_t s_openTimeMs     = 10000;    // default 10s pre-calibration
static uint32_t s_closeTimeMs    = 10000;
static bool     s_calibrated     = false;
static String   s_faultReason    = "";

// for calibration: track which phase we're in
static bool     s_calMeasureNext = false;    // after CAL_CLOSING home, swing open & measure

// ---------- low-level drive helpers ----------
static void driveStop() {
  digitalWrite(VALVE_OPEN_PIN,  LOW);
  digitalWrite(VALVE_CLOSE_PIN, LOW);
}
static void driveOpen() {
  digitalWrite(VALVE_CLOSE_PIN, LOW);
  delay(VALVE_REVERSE_DWELL_MS);  // dwell before reversing
  digitalWrite(VALVE_OPEN_PIN,  HIGH);
}
static void driveClose() {
  digitalWrite(VALVE_OPEN_PIN,  LOW);
  delay(VALVE_REVERSE_DWELL_MS);
  digitalWrite(VALVE_CLOSE_PIN, HIGH);
}
static bool readOpenLimit()   { return digitalRead(VALVE_LIMIT_OPEN_PIN)   == LOW; }
static bool readClosedLimit() { return digitalRead(VALVE_LIMIT_CLOSED_PIN) == LOW; }

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
  pinMode(VALVE_LIMIT_OPEN_PIN,   INPUT_PULLUP);
  pinMode(VALVE_LIMIT_CLOSED_PIN, INPUT_PULLUP);

  storage::loadValveCalib(s_openTimeMs, s_closeTimeMs, s_calibrated);
  Serial.printf("[VALVE] init. calibrated=%d open=%lums close=%lums\n",
                s_calibrated, (unsigned long)s_openTimeMs, (unsigned long)s_closeTimeMs);

  // Boot home: drive to closed unless already at closed limit
  if (readClosedLimit()) {
    s_position = 0; s_state = ST_IDLE;
    Serial.println("[VALVE] already at closed limit, homed");
  } else {
    s_position = 50;  // best guess until we hit the closed limit
    s_state = ST_BOOT_HOMING;
    s_moveStart = millis();
    driveClose();
    Serial.println("[VALVE] homing to closed...");
  }
  s_lastTick = millis();
  currentValvePos = (uint8_t)s_position;
}

uint8_t valve::getPosition() {
  if (s_position < 0) return 0;
  if (s_position > 100) return 100;
  return (uint8_t)(s_position + 0.5f);
}

void valve::setPosition(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (s_state == ST_FAULT) {
    Serial.println("[VALVE] setPosition ignored: FAULT (clear it first)");
    return;
  }
  if (s_state == ST_UNKNOWN || s_state == ST_BOOT_HOMING) {
    s_target = pct;  // queue for after home completes
    return;
  }
  if (s_state == ST_CAL_OPENING || s_state == ST_CAL_CLOSING) {
    Serial.println("[VALVE] setPosition ignored: calibrating");
    return;
  }
  s_target = pct;
  float delta = (float)pct - s_position;
  if (fabsf(delta) < VALVE_DEADBAND_PCT) { driveStop(); s_state = ST_IDLE; return; }

  s_moveStart = millis();
  if (delta > 0) { s_state = ST_OPENING; driveOpen(); }
  else           { s_state = ST_CLOSING; driveClose(); }
  Serial.printf("[VALVE] target=%u%% from %.1f%% (%s)\n",
                pct, s_position, delta > 0 ? "OPEN" : "CLOSE");
}

void valve::startCalibration() {
  if (s_state == ST_FAULT) return;
  Serial.println("[VALVE] CALIBRATION started: home -> open -> close");
  s_calMeasureNext = true;
  s_moveStart = millis();
  s_state = ST_CAL_CLOSING;   // first phase: drive home (closed)
  driveClose();
}

void valve::emergencyStop() {
  driveStop();
  s_state = ST_IDLE;
  s_target = (uint8_t)(s_position + 0.5f);
}

void valve::clearFault() {
  if (s_state != ST_FAULT) return;
  s_faultReason = "";
  // re-home on recovery
  if (readClosedLimit()) { s_position = 0; s_state = ST_IDLE; }
  else { s_state = ST_BOOT_HOMING; s_moveStart = millis(); driveClose(); }
}

Status valve::getStatus() {
  Status st;
  st.state          = s_state;
  st.position       = s_position;
  st.target         = s_target;
  st.calibrated     = s_calibrated;
  st.atOpenLimit    = readOpenLimit();
  st.atClosedLimit  = readClosedLimit();
  st.openTimeMs     = s_openTimeMs;
  st.closeTimeMs    = s_closeTimeMs;
  st.faultReason    = s_faultReason;
  return st;
}

// ---------- poll: the actual motion controller ----------
void valve::poll() {
  uint32_t now = millis();
  uint32_t dt  = now - s_lastTick;
  s_lastTick = now;

  bool lo = readOpenLimit();
  bool lc = readClosedLimit();

  // simultaneous limits = wiring or hardware fault
  if (lo && lc && s_state != ST_FAULT) { enterFault("both limits asserted"); }

  switch (s_state) {
    case ST_UNKNOWN:
    case ST_IDLE:
    case ST_FAULT:
      // even when idle, snap position if we're sitting on a limit
      if (lo) s_position = 100;
      if (lc) s_position = 0;
      break;

    case ST_BOOT_HOMING:
      if (lc) {
        s_position = 0; driveStop(); s_state = ST_IDLE;
        Serial.println("[VALVE] homed at closed limit");
      } else if (now - s_moveStart > VALVE_MAX_TRAVEL_MS) {
        enterFault("boot home: closed limit never reached");
      }
      break;

    case ST_OPENING: {
      s_position += (float)dt * 100.0f / (float)s_openTimeMs;
      if (lo)             { s_position = 100; driveStop(); s_state = ST_IDLE; break; }
      if (s_position >= (float)s_target - VALVE_DEADBAND_PCT) {
        driveStop(); s_state = ST_IDLE;
        if (s_position > 100) s_position = 100;
      }
      if (lc) { enterFault("closed limit hit while opening"); break; }
      if (now - s_moveStart > VALVE_MAX_TRAVEL_MS) { enterFault("opening: max travel exceeded"); }
      break;
    }

    case ST_CLOSING: {
      s_position -= (float)dt * 100.0f / (float)s_closeTimeMs;
      if (lc)             { s_position = 0;  driveStop(); s_state = ST_IDLE; break; }
      if (s_position <= (float)s_target + VALVE_DEADBAND_PCT) {
        driveStop(); s_state = ST_IDLE;
        if (s_position < 0) s_position = 0;
      }
      if (lo) { enterFault("open limit hit while closing"); break; }
      if (now - s_moveStart > VALVE_MAX_TRAVEL_MS) { enterFault("closing: max travel exceeded"); }
      break;
    }

    case ST_CAL_CLOSING: {
      // Phase 1 of cal: home to closed (don't time this phase, just home)
      // Phase 3 of cal: timed close from fully-open to closed
      if (lc) {
        if (s_calMeasureNext) {
          // home reached, now start the timed OPEN phase
          s_position = 0;
          s_moveStart = now;
          s_state = ST_CAL_OPENING;
          driveOpen();
          s_calMeasureNext = false;
          Serial.println("[VALVE] cal: homed, timing open...");
        } else {
          // phase 3 done: record close time, finish calibration
          uint32_t newCloseMs = now - s_moveStart;
          if (newCloseMs < 500 || newCloseMs > VALVE_MAX_TRAVEL_MS) {
            enterFault("cal: implausible close time"); break;
          }
          s_closeTimeMs = newCloseMs;
          s_position    = 0;
          s_calibrated  = true;
          driveStop(); s_state = ST_IDLE;
          storage::saveValveCalib(s_openTimeMs, s_closeTimeMs, true);
          Serial.printf("[VALVE] CAL complete: open=%lums close=%lums\n",
                        (unsigned long)s_openTimeMs, (unsigned long)s_closeTimeMs);
        }
      } else if (now - s_moveStart > VALVE_MAX_TRAVEL_MS) {
        enterFault("cal closing: timeout");
      }
      break;
    }

    case ST_CAL_OPENING: {
      if (lo) {
        // phase 2 done: record open time, then immediately drive close (phase 3)
        uint32_t newOpenMs = now - s_moveStart;
        if (newOpenMs < 500 || newOpenMs > VALVE_MAX_TRAVEL_MS) {
          enterFault("cal: implausible open time"); break;
        }
        s_openTimeMs = newOpenMs;
        s_position   = 100;
        s_moveStart  = now;
        s_state      = ST_CAL_CLOSING;   // phase 3
        driveClose();
        Serial.printf("[VALVE] cal: open=%lums, now timing close...\n",
                      (unsigned long)newOpenMs);
      } else if (now - s_moveStart > VALVE_MAX_TRAVEL_MS) {
        enterFault("cal opening: timeout");
      }
      break;
    }
  }

  // clamp & publish
  if (s_position < 0)   s_position = 0;
  if (s_position > 100) s_position = 100;
  currentValvePos = (uint8_t)(s_position + 0.5f);
}
