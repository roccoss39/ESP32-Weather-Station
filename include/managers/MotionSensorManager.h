#ifndef MOTION_SENSOR_MANAGER_H
#define MOTION_SENSOR_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config/timing_config.h"
#include "config/hardware_config.h" 
#include "managers/SystemManager.h" 

// Deklarujemy, że sysManager istnieje (zdefiniowany w main.cpp)
extern SystemManager sysManager;

// ==========================================
// DEFINICJA ENUMA (Musi być poza klasą)
// ==========================================
enum DisplayState {
  DISPLAY_SLEEPING = 0,   // Wyświetlacz wyłączony, czeka na ruch
  DISPLAY_ACTIVE = 1,     // Wyświetlacz aktywny, pokazuje dane
  DISPLAY_TIMEOUT = 2     // Przejście do sleep mode
};

class MotionSensorManager {
// ==========================================
// SEKCJA PRYWATNA (Zmienne i funkcja pomocnicza)
// ==========================================
private:
    volatile bool motionDetected = false;
    DisplayState currentDisplayState = DISPLAY_SLEEPING;
    unsigned long lastMotionTime = 0;
    unsigned long lastDebounce = 0;
    unsigned long ledFlashStartTime = 0;
    bool ledFlashActive = false;
    unsigned long lastSleepTime = 0; // Dla Ghost Touch Protection
    uint8_t statusLedPin = 255;  // Cache pinu LED (inicjalizowany w konstruktorze)

    // Pomocnicza funkcja do wchodzenia w Deep Sleep (wewnętrzna)
// Pomocnicza funkcja do wchodzenia w Deep Sleep (wewnętrzna)
    void enterDeepSleep() {
        Serial.println("💤 DEEP SLEEP START...");
        Serial.flush();
        
        // Konfiguracja wybudzania PIR
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);

        // === NASTAWIENIE BUDZIKA (Dla GithubUpdateManager) ===
        // Obliczamy ile czasu zostało do godziny zdefiniowanej w configu
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            
            // UŻYWAMY STAŁEJ Z CONFIGU:
            int targetMinutes = (FIRMWARE_UPDATE_HOUR * 60) + FIRMWARE_UPDATE_MINUTE; 
            
            int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
            
            long secondsToSleep = 0;
            
            if (currentMinutes < targetMinutes) {
                // Jest np. 01:00, budzimy się za 2h
                secondsToSleep = (targetMinutes - currentMinutes) * 60;
            } else {
                // Jest np. 23:00, budzimy się jutro (doba ma 1440 min)
                secondsToSleep = ((24 * 60) - currentMinutes + targetMinutes) * 60;
            }
            
            // Odejmij sekundy dla precyzji
            secondsToSleep -= timeinfo.tm_sec;

            if (secondsToSleep > 0) {
                Serial.printf("⏰ Timer ustawiony na %d:00 (za %ld s)\n", FIRMWARE_UPDATE_HOUR, secondsToSleep);
                esp_sleep_enable_timer_wakeup(secondsToSleep * 1000000ULL);
            }
        }

        // Dobranoc
        esp_deep_sleep_start();
    }

// ==========================================
// SEKCJA PUBLICZNA (Funkcje dostępne dla main.cpp)
// ==========================================
public:
    MotionSensorManager() {
        currentDisplayState = DISPLAY_ACTIVE;
        lastMotionTime = millis();
        
        // Cache pinu LED raz przy inicjalizacji
        statusLedPin = getStatusLedPin();
        if (statusLedPin != 255) {
            pinMode(statusLedPin, OUTPUT);
            digitalWrite(statusLedPin, LOW);
        }
    }

    // Gettery i Settery
    bool isMotionActive() const { return (millis() - lastMotionTime) <= SCREEN_AUTO_OFF_MS; }
    DisplayState getDisplayState() const { return currentDisplayState; }
    
    // Ghost Touch Protection
    bool isGhostTouchProtectionActive() const {
        if (currentDisplayState == DISPLAY_SLEEPING) {
            if (millis() - lastSleepTime < 1500) return true;
        }
        return false;
    }

    void initPIRHardware() {
        pinMode(PIR_PIN, INPUT);
        // statusLedPin już zainicjalizowany w konstruktorze
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

    // --- GŁÓWNA PĘTLA LOGIKI HYBRYDOWEJ ---
    void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive = false) {
        
        // 1. Obsługa LED
        if (ledFlashActive && (millis() - ledFlashStartTime) > LED_FLASH_DURATION) {
            if (statusLedPin != 255) {
                digitalWrite(statusLedPin, LOW);
            }
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

        // 3. Logika Hybrydowa
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
        lastSleepTime = millis(); // Zapisz czas dla Ghost Touch

        // KROK 1: Wygaś ekran (Fade Out)
        sysManager.fadeBacklight(sysManager.getCurrentBrightness(), 0);
        tft.writecommand(TFT_DISPOFF);
        Serial.println("🌑 Ekran wygaszony.");

        // KROK 2: DECYZJA - Hybryda czy Full Sleep?
        #if USE_HYBRID_SLEEP == 1
            // === TRYB HYBRYDOWY (1) ===
            // Tylko w nocy idziemy w Deep Sleep. W dzień CPU czuwa.
            if (sysManager.isNightDeepSleepTime()) {
                enterDeepSleep();
            } else {
                Serial.println("☁️ DZIEŃ: Light Sleep (CPU on, Screen off)");
            }
        #else
            // === TRYB FULL SLEEP (0) ===
            // Zawsze idziemy w Deep Sleep
            Serial.println("💤 FULL SLEEP MODE: Going to Deep Sleep.");
            enterDeepSleep();
        #endif
    }
};

#endif