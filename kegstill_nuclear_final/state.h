// state.h - shared data structures and global state
#pragma once
#include <Arduino.h>
#include <vector>

enum Stage : uint8_t {
  STAGE_IDLE     = 0,
  STAGE_HEATUP   = 1,
  STAGE_HEADS    = 2,
  STAGE_HEARTS   = 3,
  STAGE_TAILS    = 4,
  STAGE_SHUTDOWN = 5
};

struct Profile {
  String name;
  float  targetTemp;        // °F - hearts setpoint
  float  maxPower;          // % - max power during hearts
  float  kp;                // gain for hearts P-control
  float  cutTemp;           // °F - hard shutdown
  // Multi-stage extensions:
  float  heatupPower;       // % during HEATUP
  float  headsPower;        // % held during HEADS
  uint16_t headsDuration_s; // seconds in HEADS
  float  tailsTemp;         // °F - transition Hearts -> Tails
  float  tailsPower;        // % during TAILS
  // Per-stage motorized ball valve setpoint (0-100). 100=fully open.
  uint8_t valveHeatup;
  uint8_t valveHeads;
  uint8_t valveHearts;
  uint8_t valveTails;
};

struct BatchInfo {
  float   washABV     = 10.0f;
  float   volume      = 10.0f;
  String  unit        = "gal";
  String  ingredients = "";
  String  notes       = "";
};

struct Reading {
  unsigned long ts;   // ms since startMillis
  float temp;         // °C (raw from probe)
  float power;        // %
  float abv;          // estimated %
  uint8_t stage;
  uint8_t valve;      // 0-100 valve position
};

// ========== Global state (defined in state.cpp via .ino) ==========
extern std::vector<Profile>  profiles;
extern std::vector<Reading>  sessionReadings;
extern BatchInfo             currentBatch;

extern int   currentProfileIndex;
extern float currentTempC;
extern float currentPower;
extern float targetPower;
extern uint8_t currentValvePos;       // 0-100, motorized ball valve setpoint
extern bool  valveAutoFollowStage;    // if true, auto-set valve from profile stage
extern bool  automationEnabled;
extern bool  isRunning;
extern bool  estopActive;
extern Stage currentStage;

extern unsigned long startMillis;
extern unsigned long stageStartMillis;
extern unsigned long resumeOffsetMs;     // for resumed runs
extern unsigned long lastControlUpdate;
extern unsigned long lastStatusUpdate;
extern unsigned long lastPersistUpdate;
extern unsigned long windowStartTime;

extern bool          bleTempValid;
extern unsigned long lastBleUpdate;

// resume-pending: set true at boot if NVS shows an unfinished run
extern bool          resumePending;
extern unsigned long resumeElapsedSec;
extern uint8_t       resumeStage;
extern int           resumeProfileIdx;

// helpers
String stageName(Stage s);
float  estimateABV(float tempC);
