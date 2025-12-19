#ifndef MOTION_SENSOR_MANAGER_H
#define MOTION_SENSOR_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// === ENUM STANU WYŚWIETLACZA ===
enum DisplayState {
  DISPLAY_SLEEPING = 0,   // Wyświetlacz wyłączony, czeka na ruch
  DISPLAY_ACTIVE = 1,     // Wyświetlacz aktywny, pokazuje dane
  DISPLAY_TIMEOUT = 2     // Przejście do sleep mode
};
#include "config/timing_config.h"
#include "config/hardware_config.h" // Używamy configu
#include "managers/SystemManager.h" // Używamy SystemManagera

// Deklarujemy, że sysManager istnieje (zdefiniowany w main.cpp)
extern SystemManager sysManager;

class MotionSensorManager {
private:
    volatile bool motionDetected = false;
    DisplayState currentDisplayState = DISPLAY_SLEEPING;
    unsigned long lastMotionTime = 0;
    unsigned long lastDebounce = 0;
    unsigned long ledFlashStartTime = 0;
    bool ledFlashActive = false;
    unsigned long lastSleepTime = 0;

public:
    MotionSensorManager() {
        currentDisplayState = DISPLAY_ACTIVE;
        lastMotionTime = millis();
        
        pinMode(LED_STATUS_PIN, OUTPUT);
        digitalWrite(LED_STATUS_PIN, LOW);
    }

    bool isMotionActive() const { return (millis() - lastMotionTime) <= SCREEN_AUTO_OFF_MS; }
    DisplayState getDisplayState() const { return currentDisplayState; }
    
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

    // --- GŁÓWNA PĘTLA LOGIKI HYBRYDOWEJ ---
    void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive = false) {
        
        // 1. Obsługa LED
        if (ledFlashActive && (millis() - ledFlashStartTime) > LED_FLASH_DURATION) {
            digitalWrite(LED_STATUS_PIN, LOW);
            ledFlashActive = false;
        }

        // 2. Obsługa wykrytego ruchu
        if (motionDetected) {
            motionDetected = false;
            // Jeśli ekran wygaszony -> obudź go (Light Sleep Wakeup)
            if (currentDisplayState == DISPLAY_SLEEPING) {
                wakeUpDisplay(tft);
            }
            lastMotionTime = millis();
        }

        // 3. Logika Nocna (Light Sleep -> Deep Sleep)
        // Jeśli ekran śpi, sprawdź czy nie przyszła noc
        if (currentDisplayState == DISPLAY_SLEEPING) {
            if (sysManager.isNightDeepSleepTime()) {
                sleepDisplay(tft); // To wywoła Deep Sleep
            }
            return;
        }

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

        // Włącz sterownik i przywróć jasność przez PWM
        tft.writecommand(TFT_DISPON);
        sysManager.restoreCorrectBrightness();
        
        Serial.println("🔆 WAKE UP (Light Sleep)");
    }

    void sleepDisplay(TFT_eSPI& tft) {
        currentDisplayState = DISPLAY_SLEEPING;
        lastSleepTime = millis();
        // KROK 1: Zawsze gaś ekran (Light Sleep)
        sysManager.fadeBacklight(sysManager.getCurrentBrightness(), 0);
        tft.writecommand(TFT_DISPOFF);
        Serial.println("🌑 Ekran wygaszony.");

        // KROK 2: Sprawdź czy to Noc (Deep Sleep)
        if (sysManager.isNightDeepSleepTime()) {
            Serial.println("💤 NOC: Przechodzę w DEEP SLEEP (Reset RAM)");
            Serial.flush();
            esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);
            esp_deep_sleep_start();
        } else {
            Serial.println("☁️ DZIEŃ: Tryb Standby (CPU działa, ekran off)");
        }
    }

    void initPIRHardware() {
        pinMode(PIR_PIN, INPUT);
        pinMode(LED_STATUS_PIN, OUTPUT);
    }
    // Sprawdza, czy aktywna jest ochrona przed "duchami" (1.5 sekundy po uśpieniu)
    bool isGhostTouchProtectionActive() const {
        if (currentDisplayState == DISPLAY_SLEEPING) {
            // Jeśli minęło mniej niż 1500ms od zaśnięcia -> ignoruj dotyk
            if (millis() - lastSleepTime < 1500) {
                return true;
            }
        }
        return false;
    }
    
};

#endif