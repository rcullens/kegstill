// valve.cpp - motorized ball valve placeholder.
//
// !!! PLACEHOLDER !!!
// This file exposes a clean 0-100 setpoint API but does NOT yet drive real
// hardware. When you wire up your actual actuator, fill in setPosition()
// and poll() according to its type:
//
//   Servo / linear actuator (PWM):
//     - In begin(): ledcSetup + ledcAttachPin on VALVE_OPEN_PIN
//     - In setPosition(): map 0-100 -> duty cycle, ledcWrite
//
//   2-wire DC motor + driver + limit switches:
//     - In begin(): pinMode OPEN/CLOSE as OUTPUT, FEEDBACK as INPUT (ADC)
//     - In poll(): if commanded > sensed, pulse OPEN_PIN; if <, pulse CLOSE_PIN
//     - Stop when within deadband or limit switch hits
//
//   Stepper (step/dir):
//     - In begin(): pinMode STEP/DIR as OUTPUT, home on boot
//     - In poll(): step toward commanded position, tracking steps/percent
//
// For now we just log to serial so the rest of the firmware can be tested.

#include "valve.h"
#include "config.h"
#include "state.h"

static uint8_t  s_target = 0;
static uint8_t  s_current = 0;
static uint32_t s_lastStep = 0;

void valve::begin() {
  pinMode(VALVE_OPEN_PIN,  OUTPUT); digitalWrite(VALVE_OPEN_PIN,  LOW);
  pinMode(VALVE_CLOSE_PIN, OUTPUT); digitalWrite(VALVE_CLOSE_PIN, LOW);
  // pinMode(VALVE_FEEDBACK_PIN, INPUT);  // enable when you wire feedback ADC
  s_target  = 0;
  s_current = 0;
  Serial.println("[VALVE] placeholder driver init");
}

void valve::setPosition(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (pct == s_target) return;
  s_target = pct;
  currentValvePos = pct;
  Serial.printf("[VALVE] target -> %u%%\n", pct);
  // TODO: actual hardware command goes here. Once driver is real, poll()
  // can handle slewing toward target; for now we just snap.
  s_current = pct;
}

uint8_t valve::getPosition() { return s_current; }

void valve::poll() {
  // Placeholder: real driver would step s_current toward s_target here,
  // rate-limited (e.g. 1%/100ms), reading feedback or counting steps.
  // Example skeleton:
  //   if (s_current != s_target && millis() - s_lastStep >= 100) {
  //     s_lastStep = millis();
  //     if (s_current < s_target) { digitalWrite(VALVE_OPEN_PIN, HIGH); s_current++; }
  //     else                       { digitalWrite(VALVE_CLOSE_PIN, HIGH); s_current--; }
  //     // pulse, then turn outputs off after pulseWidth
  //   } else {
  //     digitalWrite(VALVE_OPEN_PIN, LOW);
  //     digitalWrite(VALVE_CLOSE_PIN, LOW);
  //   }
  (void)s_lastStep;
}
