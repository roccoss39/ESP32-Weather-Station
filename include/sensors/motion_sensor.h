#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "managers/MotionSensorManager.h"  // For DisplayState enum


class MotionSensorManager;

// Singleton instance MotionSensorManager
MotionSensorManager& getMotionSensorManager();

void initMotionSensor();
void IRAM_ATTR motionInterrupt();
bool isMotionActive();
void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive = false);
void wakeUpDisplay(TFT_eSPI& tft);
void sleepDisplay(TFT_eSPI& tft);
DisplayState getDisplayState();

#endif