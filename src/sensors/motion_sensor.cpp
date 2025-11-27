#include "sensors/motion_sensor.h"
#include "config/display_config.h"
#include "managers/MotionSensorManager.h"
#include <esp_sleep.h>

// Singleton instance MotionSensorManager
static MotionSensorManager motionSensorManager;

MotionSensorManager& getMotionSensorManager() {
  return motionSensorManager;
}

// ❌ USUNIĘTE: 4 extern variables zastąpione MotionSensorManager class

void initMotionSensor() {
  // Deleguj do MotionSensorManager - OOP style
  getMotionSensorManager().initPIRHardware();
  
  // Attach interrupt dla motion detection - PIR_PIN z MotionSensorManager
  attachInterrupt(digitalPinToInterrupt(27), motionInterrupt, RISING);  // PIR_PIN = 27
  
  // Manager automatycznie obsługuje cold start vs wake up w konstruktorze
}

void IRAM_ATTR motionInterrupt() {
  // Deleguj do MotionSensorManager - OOP style  
  getMotionSensorManager().handleMotionInterrupt();
}

bool isMotionActive() {
  // Deleguj do MotionSensorManager - OOP style
  return getMotionSensorManager().isMotionActive();
}

void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive) {
  // Deleguj do MotionSensorManager - OOP style z przekazaniem parametru
  getMotionSensorManager().updateDisplayPowerState(tft, isConfigModeActive);
}

void wakeUpDisplay(TFT_eSPI& tft) {
  // Deleguj do MotionSensorManager - OOP style
  getMotionSensorManager().wakeUpDisplay(tft);
}

void sleepDisplay(TFT_eSPI& tft) {
  // Deleguj do MotionSensorManager - OOP style
  getMotionSensorManager().sleepDisplay(tft);
}

DisplayState getDisplayState() {
  // Deleguj do MotionSensorManager - OOP style
  return getMotionSensorManager().getDisplayState();
}