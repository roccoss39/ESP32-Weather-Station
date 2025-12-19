#ifndef MOTION_SENSOR_MANAGER_H
#define MOTION_SENSOR_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config/timing_config.h"
#include "config/hardware_config.h" 
#include "managers/SystemManager.h" 

extern SystemManager sysManager;

// Enum stanów
enum DisplayState {
  DISPLAY_SLEEPING = 0,
  DISPLAY_ACTIVE = 1,
  DISPLAY_TIMEOUT = 2
};

class MotionSensorManager {
private:
    volatile bool motionDetected = false;
    DisplayState currentDisplayState = DISPLAY_SLEEPING;
    unsigned long lastMotionTime = 0;
    unsigned long lastDebounce = 0;
    unsigned long ledFlashStartTime = 0;
    unsigned long lastSleepTime = 0; // Dla Ghost Touch Protection
    bool ledFlashActive = false;

    // Pomocnicza funkcja do wchodzenia w Deep Sleep (wspólna dla obu trybów)
    void enterDeepSleep() {
        Serial.println("💤 DEEP SLEEP START (Reset RAM, CPU OFF)");
        Serial.flush();
        // Konfiguracja wybudzania PIR
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);
        // Dobranoc
        esp_deep_sleep_start();
    }

public:
    MotionSensorManager() {
        // Przy Deep Sleep (Full Mode) każde wybudzenie to restart, więc
        // zawsze startujemy jako ACTIVE.
        currentDisplayState = DISPLAY_ACTIVE;
        lastMotionTime = millis();
        
        pinMode(LED_STATUS_PIN, OUTPUT);
        digitalWrite(LED_STATUS_PIN, LOW);
    }

    bool isMotionActive() const { return (millis() - lastMotionTime) <= SCREEN_AUTO_OFF_MS; }
    DisplayState getDisplayState() const { return currentDisplayState; }
    
    // Ghost Touch Protection (blokada dotyku przez 1.5s po wygaszeniu)
    bool isGhostTouchProtectionActive() const {
        if (currentDisplayState == DISPLAY_SLEEPING) {
            if (millis() - lastSleepTime < 1500) return true;
        }
        return false;
    }

    void handleMotionInterrupt() {
        unsigned long currentTime = millis();
        if (currentTime - lastDebounce < PIR_DEBOUNCE_TIME) return;
        lastDebounce = currentTime;

        motionDetected = true;
        lastMotionTime = currentTime;

        digitalWrite(LED_STATUS_PIN, HIGH);
        ledFlashActive = true;
        ledFlashStartTime = currentTime;
    }

    void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive = false) {
        
        // 1. Obsługa LED
        if (ledFlashActive && (millis() - ledFlashStartTime) > LED_FLASH_DURATION) {
            digitalWrite(LED_STATUS_PIN, LOW);
            ledFlashActive = false;
        }

        // 2. Obsługa wykrytego ruchu
        if (motionDetected) {
            motionDetected = false;
            if (currentDisplayState == DISPLAY_SLEEPING) {
                wakeUpDisplay(tft);
            }
            lastMotionTime = millis();
        }

        // 3. Specyficzna logika dla trybu HYBRYDOWEGO
        // ZMIANA: Używamy #if zamiast #ifdef i sprawdzamy czy == 1
        #if USE_HYBRID_SLEEP == 1
            if (currentDisplayState == DISPLAY_SLEEPING) {
                if (sysManager.isNightDeepSleepTime()) {
                    Serial.println("🌑 Noc nadeszła w trakcie czuwania -> Deep Sleep");
                    sleepDisplay(tft); 
                }
                return;
            }
        #endif

        // 4. Timeout (Aktywny -> Uśpij)
        unsigned long timeout = isConfigModeActive ? CONFIG_MODE_TIMEOUT_MS : SCREEN_AUTO_OFF_MS;
        if (currentDisplayState == DISPLAY_ACTIVE && (millis() - lastMotionTime > timeout)) {
            sleepDisplay(tft);
        }
    }

    void wakeUpDisplay(TFT_eSPI& tft) {
        if (currentDisplayState == DISPLAY_ACTIVE) return;
        
        currentDisplayState = DISPLAY_ACTIVE;
        lastMotionTime = millis();

        tft.writecommand(TFT_DISPON);
        sysManager.restoreCorrectBrightness();
        
        Serial.println("🔆 WAKE UP");
    }

    void sleepDisplay(TFT_eSPI& tft) {
        currentDisplayState = DISPLAY_SLEEPING;
        lastSleepTime = millis();

        // KROK 1: Wygaś ekran (wspólne dla obu trybów)
        sysManager.fadeBacklight(sysManager.getCurrentBrightness(), 0);
        tft.writecommand(TFT_DISPOFF);
        Serial.println("🌑 Ekran wygaszony.");

        // KROK 2: DECYZJA - Hybryda czy Full Sleep?
        
        // ZMIANA: Używamy #if zamiast #ifdef
        #if USE_HYBRID_SLEEP == 1
            // === TRYB HYBRYDOWY (1) ===
            if (sysManager.isNightDeepSleepTime()) {
                enterDeepSleep();
            } else {
                Serial.println("☁️ DZIEŃ: Light Sleep (CPU on, Screen off)");
            }
        #else
            // === TRYB FULL SLEEP (0) ===
            Serial.println("💤 FULL SLEEP MODE: Going to Deep Sleep immediately.");
            enterDeepSleep();
        #endif
    }

    void initPIRHardware() {
        pinMode(PIR_PIN, INPUT);
        pinMode(LED_STATUS_PIN, OUTPUT);
    }
};

#endif