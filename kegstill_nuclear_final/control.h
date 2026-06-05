// control.h - 4-stage automation + PWM SSR control
#pragma once
#include <Arduino.h>

namespace control {
  void begin();            // configure pins
  void update();           // run from loop every CONTROL_PERIOD_MS

  // commanded actions (mirror what the web handlers used to do inline)
  bool startRun(bool resume);   // returns false if cannot start (e.g. no probe)
  void stopRun();
  void estop();
  void resetAll();
  void setManualPower(float pct);
  void setAutomation(bool en);
  void advanceStage();          // operator skip-to-next-stage
  void reapplyStageValve();     // re-issue valve target for current stage
}
