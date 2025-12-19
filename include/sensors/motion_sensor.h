#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "managers/MotionSensorManager.h"  // For DisplayState enum

// --- KONFIGURACJA PRZENIESIONA DO MotionSensorManager.h ---
// PIR_PIN, MOTION_TIMEOUT, DEBOUNCE_TIME, DisplayState enum
// są teraz w MotionSensorManager class

// --- DODAJEMY ENUM TUTAJ dla backward compatibility ---
// DisplayState enum moved to MotionSensorManager.h

// --- NOWY OOP SYSTEM ---
// Zastąpiono 4 extern variables MotionSensorManager class
// Forward declaration zamiast include w header
class MotionSensorManager;

// Singleton instance MotionSensorManager
MotionSensorManager& getMotionSensorManager();

// --- FUNKCJE ---
void initMotionSensor();
void IRAM_ATTR motionInterrupt();
bool isMotionActive();
void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive = false);
void wakeUpDisplay(TFT_eSPI& tft);
void sleepDisplay(TFT_eSPI& tft);
DisplayState getDisplayState();

#endif