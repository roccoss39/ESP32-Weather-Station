#ifndef MOTION_SENSOR_MANAGER_H
#define MOTION_SENSOR_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "config/timing_config.h"
#include "config/hardware_config.h" 
#include "managers/SystemManager.h" 

// Deklarujemy istnienie zewnętrznych zmiennych/funkcji z main.cpp / ui
extern SystemManager sysManager;
extern void checkAndShowGreeting(TFT_eSPI& tft);
extern bool isWiFiConfigActive(); // <-- Deklaracja funkcji z wifi_touch_interface

enum DisplayState {
  DISPLAY_SLEEPING = 0,   // Wyświetlacz wyłączony, czeka na ruch
  DISPLAY_ACTIVE = 1,     // Wyświetlacz aktywny, pokazuje dane
  DISPLAY_TIMEOUT = 2     // Przejście do sleep mode
};

class MotionSensorManager {

private:
    volatile bool motionDetected = false;
    DisplayState currentDisplayState = DISPLAY_SLEEPING;
    unsigned long lastMotionTime = 0;
    unsigned long lastDebounce = 0;
    unsigned long ledFlashStartTime = 0;
    bool ledFlashActive = false;
    unsigned long lastSleepTime = 0; // Dla Ghost Touch Protection
    uint8_t statusLedPin = 255;  // Cache pinu LED

    void enterDeepSleep() {
        // --- ABSOLUTNA BLOKADA BEZPIECZEŃSTWA ---
        if (isWiFiConfigActive()) {
            Serial.println("⛔ [MotionManager] Zablokowano wejście w Deep Sleep, bo użytkownik jest w menu konfiguracyjnym!");
            return;
        }

        Serial.println("💤 DEEP SLEEP START...");
        Serial.flush();
        
        // =========================================================================
        // TARCZA OCHRONNA: ŁAGODNE WYGASZANIE SYSTEMU
        // Zapobiega skokom napięcia (Brownout/Spike), które fałszywie wyzwalają PIR
        // =========================================================================
        if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(500); // Dajemy 0.5s na rozładowanie kondensatorów i stabilizację
        }

        // =========================================================================
        // BLOKADA FAŁSZYWYCH WYBUDZEŃ (PIR STUCK HIGH PROTECTION)
        // Jeśli zaśniemy, gdy PIR jest wciąż wysoki, stacja obudzi się natychmiast!
        // =========================================================================
        pinMode(PIR_PIN, INPUT);
        int patience = 0;
        // Czekamy, aż czujnik całkowicie opadnie do zera (MAX 10 sekund)
        while(digitalRead(PIR_PIN) == HIGH && patience < 100) { 
            delay(100);
            yield();
            patience++;
        }
        
        if (patience > 0) {
            Serial.printf("🛡️ PIR ustabilizowany po %d ms. Gotowy do snu.\n", patience * 100);
        }

        // Dopiero teraz można bezpiecznie uzbroić wybudzanie na pinie!
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);

        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            int targetMinutes = (FIRMWARE_UPDATE_HOUR * 60) + FIRMWARE_UPDATE_MINUTE; 
            int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            long secondsToSleep = 0;
            
            if (currentMinutes < targetMinutes) {
                secondsToSleep = (targetMinutes - currentMinutes) * 60;
            } else {
                secondsToSleep = ((24 * 60) - currentMinutes + targetMinutes) * 60;
            }
            
            secondsToSleep -= timeinfo.tm_sec;

            if (secondsToSleep > 0) {
                Serial.printf("⏰ Timer ustawiony na %d:00 (za %ld s)\n", FIRMWARE_UPDATE_HOUR, secondsToSleep);
                esp_sleep_enable_timer_wakeup(secondsToSleep * 1000000ULL);
            }
        }
        esp_deep_sleep_start();
    }

public:
    MotionSensorManager() {
        currentDisplayState = DISPLAY_ACTIVE;
        lastMotionTime = millis();
        
        statusLedPin = getStatusLedPin();
        if (statusLedPin != 255) {
            pinMode(statusLedPin, OUTPUT);
            digitalWrite(statusLedPin, LOW);
        }
    }

    void clearMotionFlag() {
        motionDetected = false;
        Serial.println("🧹 DEBUG: Flaga PIR wyczyszczona (Ignoruję zakłócenia)");
    }

    bool isMotionActive() const { return (millis() - lastMotionTime) <= SCREEN_AUTO_OFF_MS; }
    DisplayState getDisplayState() const { return currentDisplayState; }
    
    bool isGhostTouchProtectionActive() const {
        if (currentDisplayState == DISPLAY_SLEEPING) {
            if (millis() - lastSleepTime < 1500) return true;
        }
        return false;
    }

    void initPIRHardware() {
        pinMode(PIR_PIN, INPUT);
        if (statusLedPin != 255) {
            pinMode(statusLedPin, OUTPUT);
        }
    }
    
    void handleMotionInterrupt() {
        unsigned long currentTime = millis();
        if (currentTime - lastDebounce < PIR_DEBOUNCE_TIME) return;
        lastDebounce = currentTime;

        motionDetected = true;
        lastMotionTime = currentTime;

        if (statusLedPin != 255) {
            digitalWrite(statusLedPin, HIGH);
        }
        ledFlashActive = true;
        ledFlashStartTime = currentTime;
    }

    void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActiveFlag = false) {
        
        if (ledFlashActive && (millis() - ledFlashStartTime) > LED_FLASH_DURATION) {
            if (statusLedPin != 255) {
                digitalWrite(statusLedPin, LOW);
            }
            ledFlashActive = false;
        }

        if (motionDetected) {
            motionDetected = false;
            Serial.println("🚨 DEBUG: Czujnik PIR wykrył ruch!");
            if (currentDisplayState == DISPLAY_SLEEPING) {
                wakeUpDisplay(tft);
            }
            lastMotionTime = millis();
        }

        unsigned long timeout;
        if (isConfigModeActiveFlag || isWiFiConfigActive()) {
            timeout = 180000; 
        } else {
            timeout = SCREEN_AUTO_OFF_MS; 
        }

        #if USE_HYBRID_SLEEP == 1
            if (currentDisplayState == DISPLAY_SLEEPING) {
                if (sysManager.isNightDeepSleepTime() && !isWiFiConfigActive()) {
                    Serial.println("🌑 Noc nadeszła w trakcie czuwania -> Deep Sleep");
                    sleepDisplay(tft); 
                }
                return;
            }
        #endif

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
        
        checkAndShowGreeting(tft);
    }

    void sleepDisplay(TFT_eSPI& tft) {
        if (isWiFiConfigActive()) {
            Serial.println("⚙️ [MotionManager] Wygaszenie ekranu w trakcie trybu Config - powrót do normalnego stanu.");
            extern void exitWiFiConfigMode();
            exitWiFiConfigMode(); 
        }

        currentDisplayState = DISPLAY_SLEEPING;
        lastSleepTime = millis(); 

        sysManager.fadeBacklight(sysManager.getCurrentBrightness(), 0);
        tft.writecommand(TFT_DISPOFF);
        Serial.println("🌑 Ekran wygaszony.");

        #if USE_HYBRID_SLEEP == 1
            if (sysManager.isNightDeepSleepTime() && !isWiFiConfigActive()) {
                enterDeepSleep();
            } else {
                Serial.println("☁️ DZIEŃ: Light Sleep (CPU on, Screen off)");
            }
        #else
            if (!isWiFiConfigActive()) {
                Serial.println("💤 FULL SLEEP MODE: Going to Deep Sleep.");
                enterDeepSleep();
            } else {
                Serial.println("☁️ Light Sleep (Zablokowano DeepSleep ze względu na Config Mode)");
            }
        #endif
    }
};

void clearPirFlagGlobal();

#endif