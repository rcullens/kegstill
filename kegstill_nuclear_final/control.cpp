// control.cpp - 4-stage automation + slow-PWM SSR control
#include "control.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "history.h"
#include "valve.h"

static void applyStageValve(Stage s, const Profile& p) {
  if (!valveAutoFollowStage) return;
  uint8_t pos = currentValvePos;
  switch (s) {
    case STAGE_HEATUP:   pos = p.valveHeatup; break;
    case STAGE_HEADS:    pos = p.valveHeads;  break;
    case STAGE_HEARTS:   pos = p.valveHearts; break;
    case STAGE_TAILS:    pos = p.valveTails;  break;
    case STAGE_SHUTDOWN: pos = 0; break;
    default: return;
  }
  valve::setPosition(pos);
}

static void enterStage(Stage s) {
  currentStage     = s;
  stageStartMillis = millis();
  Serial.printf("[CTRL] -> stage %s\n", stageName(s).c_str());
  if (currentProfileIndex >= 0 && currentProfileIndex < (int)profiles.size())
    applyStageValve(s, profiles[currentProfileIndex]);
}

void control::begin() {
  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, LOW);
}

bool control::startRun(bool resume) {
  if (estopActive) return false;
  if (!bleTempValid) {
    Serial.println("[CTRL] refuse start: no BLE probe");
    return false;
  }
  if (profiles.empty() || currentProfileIndex < 0 || currentProfileIndex >= (int)profiles.size()) {
    Serial.println("[CTRL] refuse start: no profile");
    return false;
  }

  isRunning   = true;
  estopActive = false;
  startMillis = millis();
  windowStartTime = millis();
  sessionReadings.clear();
  sessionReadings.reserve(MAX_SESSION_PTS);

  if (resume) {
    resumeOffsetMs = (unsigned long)resumeElapsedSec * 1000UL;
    enterStage((Stage)resumeStage);
    Serial.printf("[CTRL] resumed at %lus, stage %s\n",
                  (unsigned long)resumeElapsedSec, stageName(currentStage).c_str());
  } else {
    resumeOffsetMs = 0;
    enterStage(STAGE_HEATUP);
  }
  storage::saveRunSnapshot();
  return true;
}

void control::stopRun() {
  if (isRunning) history::saveCurrentRun();   // persist run before clearing
  isRunning = false;
  automationEnabled = false;
  currentPower = 0;
  targetPower = 0;
  digitalWrite(SSR_PIN, LOW);
  valve::setPosition(0);
  enterStage(STAGE_IDLE);
  storage::clearRunSnapshot();
}

void control::estop() {
  estopActive       = true;
  automationEnabled = false;
  currentPower      = 0;
  targetPower       = 0;
  isRunning         = false;
  digitalWrite(SSR_PIN, LOW);
  enterStage(STAGE_IDLE);
  storage::clearRunSnapshot();
}

void control::resetAll() {
  isRunning         = false;
  estopActive       = false;
  automationEnabled = false;
  currentPower      = 0;
  targetPower       = 0;
  sessionReadings.clear();
  resumeOffsetMs    = 0;
  resumePending     = false;
  digitalWrite(SSR_PIN, LOW);
  enterStage(STAGE_IDLE);
  storage::clearRunSnapshot();
}

void control::setManualPower(float pct) {
  currentPower = constrain(pct, 0.0f, 100.0f);
  targetPower  = currentPower;
  automationEnabled = false;
  storage::saveAutomation(false);
}

void control::setAutomation(bool en) {
  automationEnabled = en;
  storage::saveAutomation(en);
}

void control::advanceStage() {
  if (!isRunning) return;
  Stage next = currentStage;
  switch (currentStage) {
    case STAGE_HEATUP:   next = STAGE_HEADS;    break;
    case STAGE_HEADS:    next = STAGE_HEARTS;   break;
    case STAGE_HEARTS:   next = STAGE_TAILS;    break;
    case STAGE_TAILS:    next = STAGE_SHUTDOWN; break;
    default: return;
  }
  enterStage(next);
}

void control::reapplyStageValve() {
  if (currentProfileIndex < 0 || currentProfileIndex >= (int)profiles.size()) return;
  applyStageValve(currentStage, profiles[currentProfileIndex]);
}

// ---------- main loop ----------
void control::update() {
  // Safety: not running -> SSR off, nothing else to do
  if (!isRunning || estopActive) {
    if (digitalRead(SSR_PIN) == HIGH) digitalWrite(SSR_PIN, LOW);
    return;
  }

  // Probe sanity
  if (!bleTempValid) {
    Serial.println("[CTRL] ESTOP: probe never valid");
    estop();
    return;
  }
  if (millis() - lastBleUpdate > BLE_TIMEOUT_MS) {
    Serial.println("[CTRL] ESTOP: probe timeout");
    estop();
    return;
  }
  if (currentTempC > HARD_MAX_TEMP_C || currentTempC < HARD_MIN_TEMP_C) {
    Serial.printf("[CTRL] ESTOP: temp %.1fC out of bounds\n", currentTempC);
    estop();
    return;
  }
  unsigned long elapsedMs = (millis() - startMillis) + resumeOffsetMs;
  if (elapsedMs > MAX_RUN_MS) {
    Serial.println("[CTRL] ESTOP: max runtime");
    estop();
    return;
  }

  // Pick power based on stage (only when automation is on)
  if (automationEnabled && currentProfileIndex >= 0 && currentProfileIndex < (int)profiles.size()) {
    Profile& p   = profiles[currentProfileIndex];
    float tempF  = cToF(currentTempC);

    // Hard cut: any time we cross cutTemp, force SHUTDOWN
    if (tempF >= p.cutTemp && currentStage != STAGE_SHUTDOWN) {
      Serial.printf("[CTRL] cut temp %.1fF reached, SHUTDOWN\n", tempF);
      enterStage(STAGE_SHUTDOWN);
    }

    switch (currentStage) {
      case STAGE_HEATUP: {
        currentPower = constrain(p.heatupPower, 0.0f, 100.0f);
        // when vapor is within 5°F of target, move to HEADS
        if (tempF >= (p.targetTemp - 5.0f)) enterStage(STAGE_HEADS);
        break;
      }
      case STAGE_HEADS: {
        currentPower = constrain(p.headsPower, 0.0f, 100.0f);
        if ((millis() - stageStartMillis) / 1000UL >= p.headsDuration_s) enterStage(STAGE_HEARTS);
        break;
      }
      case STAGE_HEARTS: {
        // proper P-control around headsPower baseline, clamped to maxPower
        float error = p.targetTemp - tempF;
        float cmd   = p.headsPower + error * p.kp;
        currentPower = constrain(cmd, 0.0f, p.maxPower);
        if (tempF >= p.tailsTemp) enterStage(STAGE_TAILS);
        break;
      }
      case STAGE_TAILS: {
        currentPower = constrain(p.tailsPower, 0.0f, 100.0f);
        // SHUTDOWN handled by cutTemp check above
        break;
      }
      case STAGE_SHUTDOWN: {
        currentPower = 0.0f;
        valve::setPosition(0);
        history::saveCurrentRun();    // persist completed run
        isRunning    = false;
        digitalWrite(SSR_PIN, LOW);
        storage::clearRunSnapshot();
        return;
      }
      default: break;
    }
    targetPower = currentPower;
  }
  // (when automation is OFF, currentPower stays at whatever the slider set)

  // ---------- slow-PWM window for SSR ----------
  unsigned long now = millis();
  if (now - windowStartTime >= PWM_WINDOW_MS) windowStartTime += PWM_WINDOW_MS;
  unsigned long onTime = (unsigned long)((currentPower / 100.0f) * PWM_WINDOW_MS);
  digitalWrite(SSR_PIN, (now - windowStartTime) < onTime ? HIGH : LOW);
}
