#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// --- KONFIGURACJA PRZENIESIONA DO MotionSensorManager.h ---
// PIR_PIN, MOTION_TIMEOUT, DEBOUNCE_TIME, DisplayState enum
// są teraz w MotionSensorManager class

// --- DODAJEMY ENUM TUTAJ dla backward compatibility ---
enum DisplayState {
  DISPLAY_SLEEPING = 0,   // Wyświetlacz wyłączony, czeka na ruch
  DISPLAY_ACTIVE = 1,     // Wyświetlacz aktywny, pokazuje dane
  DISPLAY_TIMEOUT = 2     // Przejście do sleep mode
};

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