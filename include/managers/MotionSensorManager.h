#ifndef MOTION_SENSOR_MANAGER_H
#define MOTION_SENSOR_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include "config/timing_config.h"
#include "config/hardware_config.h" 
#include "managers/SystemManager.h" 

// Import zewnętrznych funkcji i flag
extern SystemManager sysManager;
extern void checkAndShowGreeting(TFT_eSPI& tft);
extern bool isWiFiConfigActive();
extern bool isImageDownloadInProgress; // Flaga chroniąca przed zakłóceniami podczas pobierania NASA

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
    bool ledFlashActive = false;
    unsigned long lastSleepTime = 0; 
    uint8_t statusLedPin = 255;  
    unsigned long hardwareInitTime = 0; // Czas inicjalizacji sprzętu

    void enterDeepSleep() {
        if (isWiFiConfigActive()) {
            Serial.println("⛔ [MotionManager] Zablokowano wejście w Deep Sleep, bo użytkownik jest w menu konfiguracyjnym!");
            return;
        }

        Serial.println("💤 DEEP SLEEP START...");
        Serial.flush();
        
        // Łagodne odcięcie WiFi
        if (WiFi.status() == WL_CONNECTED || WiFi.getMode() != WIFI_OFF) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            delay(500); 
        }

        // Oczekiwanie na opadnięcie pinu PIR
        pinMode(PIR_PIN, INPUT);
        int patience = 0;
        while(digitalRead(PIR_PIN) == HIGH && patience < 100) { 
            delay(100);
            yield();
            patience++;
        }
        
        if (patience > 0) {
            Serial.printf("🛡️ PIR ustabilizowany po %d ms. Gotowy do snu.\n", patience * 100);
        }

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
        Serial.println("🧹 DEBUG: Flaga PIR wyczyszczona programowo.");
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
        hardwareInitTime = millis();
        motionDetected = false; // Czyszczenie śmieci ze startu
    }
    
    void handleMotionInterrupt() {
        unsigned long currentTime = millis();
        
        // --- POZIOM 0: BLOKADA "OGONA" PO WYBUDZENIU ---
        if (currentTime - hardwareInitTime < 8000) return;

        // --- POZIOM 1: BLOKADA PODCZAS TRANSFERU ---
        if (isImageDownloadInProgress) {
            return; // Gdy trwa pobieranie obrazka, absolutnie ignorujemy wszystko!
        }

        // Debounce sprzętowy
        if (currentTime - lastDebounce < 2000) return; 
        lastDebounce = currentTime;

        motionDetected = true;
    }

    void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActiveFlag = false) {
        
        if (ledFlashActive && (millis() - ledFlashStartTime) > 200) {
            if (statusLedPin != 255) {
                digitalWrite(statusLedPin, LOW);
            }
            ledFlashActive = false;
        }

        if (motionDetected) {
            motionDetected = false; 
            
            // --- POZIOM 2: WERYFIKACJA PRAWDZIWOŚCI RUCHU W TRAKCIE PRACY ---
            bool isRealMotion = true;
            
            for(int i = 0; i < 50; i++) {
                if (digitalRead(PIR_PIN) == LOW) {
                    isRealMotion = false; 
                    break;
                }
                delay(10);
            }

            if (!isRealMotion) {
                Serial.println("👻 [PIR] Zignorowano szpilke napiecia w locie (TFT/WiFi spike)");
            } 
            else if (isImageDownloadInProgress) {
                Serial.println("📡 [PIR] Zignorowano ruch (Trwa wciaz transfer WiFi)");
            }
            else {
                Serial.println("🚨 DEBUG: Czujnik PIR wykryl PRAWDZIWY ruch!");
                
                if (statusLedPin != 255) {
                    digitalWrite(statusLedPin, HIGH);
                }
                ledFlashActive = true;
                ledFlashStartTime = millis();

                if (currentDisplayState == DISPLAY_SLEEPING) {
                    wakeUpDisplay(tft);
                }
                lastMotionTime = millis();
            }
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
            } 
        #else
            if (!isWiFiConfigActive()) {
                enterDeepSleep();
            } 
        #endif
    }
};

void clearPirFlagGlobal();

#endif