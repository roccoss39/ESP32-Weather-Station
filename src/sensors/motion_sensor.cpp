#include "sensors/motion_sensor.h"
#include "config/hardware_config.h"
#include "config/display_config.h"
#include "managers/MotionSensorManager.h"
#include <esp_sleep.h>

// Singleton
static MotionSensorManager motionSensorManager;

MotionSensorManager& getMotionSensorManager() {
  return motionSensorManager;
}

void initMotionSensor() {
  getMotionSensorManager().initPIRHardware();
  // Używamy pinu z configu
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), motionInterrupt, RISING);  
}

void IRAM_ATTR motionInterrupt() {
  getMotionSensorManager().handleMotionInterrupt();
}

bool isMotionActive() {
  return getMotionSensorManager().isMotionActive();
}

void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive) {
  getMotionSensorManager().updateDisplayPowerState(tft, isConfigModeActive);
}

void wakeUpDisplay(TFT_eSPI& tft) {
  getMotionSensorManager().wakeUpDisplay(tft);
}

void sleepDisplay(TFT_eSPI& tft) {
  getMotionSensorManager().sleepDisplay(tft);
}

DisplayState getDisplayState() {
  return getMotionSensorManager().getDisplayState();
}