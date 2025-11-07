#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// --- KONFIGURACJA PIR MOD-01655 ---
#define PIR_PIN 27                    // GPIO pin dla MOD-01655
#define MOTION_TIMEOUT 30000          // 30 sekund timeout bez ruchu
#define DEBOUNCE_TIME 500            // 500ms debounce dla stabilności

// --- STANY WYŚWIETLACZA ---
enum DisplayState {
  DISPLAY_SLEEPING = 0,   // Wyświetlacz wyłączony, czeka na ruch
  DISPLAY_ACTIVE = 1,     // Wyświetlacz aktywny, pokazuje dane
  DISPLAY_TIMEOUT = 2     // Przejście do sleep mode
};

// --- ZMIENNE GLOBALNE ---
extern volatile bool motionDetected;
extern DisplayState currentDisplayState;
extern unsigned long lastMotionTime;
extern unsigned long lastDisplayUpdate;

// --- FUNKCJE ---
void initMotionSensor();
void IRAM_ATTR motionInterrupt();
bool isMotionActive();
void updateDisplayPowerState(TFT_eSPI& tft);
void wakeUpDisplay(TFT_eSPI& tft);
void sleepDisplay(TFT_eSPI& tft);
DisplayState getDisplayState();

#endif