#ifndef MOTION_SENSOR_MANAGER_H
#define MOTION_SENSOR_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_sleep.h>
#include "config/timing_config.h"

// Forward declaration - enum jest już w motion_sensor.h

// Hardware config
#define PIR_PIN 27
#define MOTION_TIMEOUT 60000    // 60 sekund (1 minuta) timeout bez ruchu
#define DEBOUNCE_TIME 500       // 500ms debounce dla stabilności
#define LED_BUILTIN 2           // Wbudowana niebieska dioda ESP32
#define LED_FLASH_DURATION 1000 // 1 sekunda świecenia diody

/**
 * 🔍 MotionSensorManager - Smart PIR sensor + state management
 * 
 * Zastępuje 4 extern variables + business logic lepszą enkapsulacją:
 * - volatile bool motionDetected
 * - DisplayState currentDisplayState
 * - unsigned long lastMotionTime
 * - unsigned long lastDisplayUpdate
 * 
 * Plus zarządza całą logiką PIR:
 * - Interrupt handling
 * - State transitions (SLEEPING/ACTIVE/TIMEOUT)
 * - Deep sleep management
 * - Timeout calculations
 */
class MotionSensorManager {
private:
    // --- PRIVATE STATE ---
    volatile bool motionDetected = false;
    DisplayState currentDisplayState = DISPLAY_SLEEPING;
    unsigned long lastMotionTime = 0;
    unsigned long lastDisplayUpdate = 0;
    unsigned long lastDebounce = 0;
    unsigned long ledFlashStartTime = 0;
    bool ledFlashActive = false;

public:
    // --- CONSTRUCTOR ---
    MotionSensorManager() {
        // Initialize with cold start state
        esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
        
        if (wakeupReason == ESP_SLEEP_WAKEUP_EXT0) {
            // PIR wake up - display aktywny
            currentDisplayState = DISPLAY_ACTIVE;
            lastMotionTime = millis();
            Serial.println("🔥 PIR WAKE UP - MotionSensorManager ACTIVE");
        } else {
            // Cold start - display aktywny na 60s demo
            currentDisplayState = DISPLAY_ACTIVE;
            lastMotionTime = millis();
            Serial.println("🚀 COLD START - MotionSensorManager demo 60s");
        }
        
        lastDisplayUpdate = millis();
        
        // Inicjalizuj GPIO dla wbudowanej diody
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW); // Początkowy stan wyłączony
    }
    
    // --- GETTERS ---
    bool isMotionDetected() const { return motionDetected; }
    DisplayState getDisplayState() const { return currentDisplayState; }
    unsigned long getLastMotionTime() const { return lastMotionTime; }
    unsigned long getLastDisplayUpdate() const { return lastDisplayUpdate; }
    
    // --- SETTERS ---
    void setMotionDetected(bool detected) { motionDetected = detected; }
    void setDisplayState(DisplayState state) { currentDisplayState = state; }
    
    // --- BUSINESS LOGIC ---
    
    /**
     * Sprawdza czy minął timeout od ostatniego ruchu
     * @return true jeśli timeout minął
     */
    bool isMotionTimeout() const {
        return (millis() - lastMotionTime) > MOTION_TIMEOUT;
    }
    
    /**
     * Sprawdza czy motion jest aktywny (w ramach timeout)
     * @return true jeśli motion jest aktywny
     */
    bool isMotionActive() const {
        return !isMotionTimeout();
    }
    
    /**
     * Aktualizuje czas ostatniego ruchu
     */
    void updateMotionTime() {
        lastMotionTime = millis();
    }
    
    /**
     * Aktualizuje czas ostatniej aktualizacji display
     */
    void updateDisplayTime() {
        lastDisplayUpdate = millis();
    }
    
    /**
     * Obsługuje interrupt od PIR - IRAM_ATTR compatible
     * Wywołuj z motionInterrupt()
     */
    void handleMotionInterrupt() {
        unsigned long currentTime = millis();
        
        // Debounce protection
        if (currentTime - lastDebounce < DEBOUNCE_TIME) {
            return;
        }
        lastDebounce = currentTime;
        
        // Set motion detected
        motionDetected = true;
        lastMotionTime = currentTime;
        
        // 💙 ZAPAL NIEBIESKĄ DIODĘ na chwilę
        digitalWrite(LED_BUILTIN, HIGH);
        ledFlashActive = true;
        ledFlashStartTime = currentTime;
        
        // DEBUG: Log motion detection z więcej szczegółów
        // Serial.printf("🔥 PIR MOTION DETECTED! Timer reset to %lu ms (was %lu ms since last motion)\n", 
        //               currentTime, currentTime - lastMotionTime);
        
        // Wake up display if sleeping
        if (currentDisplayState == DISPLAY_SLEEPING) {
            currentDisplayState = DISPLAY_ACTIVE;
           // Serial.println("🔥 MOTION DETECTED - Wake up display!");
        } else {
          //  Serial.println("🔄 MOTION DETECTED - Extending display time");
        }
    }
    
    /**
     * Główna logika zarządzania mocą display
     * Wywołuj w każdym loop()
     * @param isConfigModeActive - true gdy WiFi config aktywny (unika touch race condition)
     */
    /**
 * Główna logika zarządzania mocą display
 * Wywołuj w każdym loop()
 * @param isConfigModeActive - true gdy WiFi config aktywny (unika touch race condition)
 */
/**
 * Główna logika zarządzania mocą display
 * Wywołuj w każdym loop()
 * @param isConfigModeActive - true gdy WiFi config aktywny (unika touch race condition)
 */
void updateDisplayPowerState(TFT_eSPI& tft, bool isConfigModeActive = false) {

    // --- KROK 0: Sprawdź czy wyłączyć diodę po wykryciu ruchu ---
    if (ledFlashActive && (millis() - ledFlashStartTime) > LED_FLASH_DURATION) {
        digitalWrite(LED_BUILTIN, LOW); // Wyłącz diodę
        ledFlashActive = false;
        Serial.println("💙 LED wyłączona po flash");
    }

    // --- KROK 1: Sprawdź aktywność DOTYKU (tylko w trybie NORMALNYM) ---
    // (W trybie WiFi dotyk jest sprawdzany w wifi_touch_interface.cpp,
    //  co zapobiega konfliktowi "race condition")
    if (!isConfigModeActive) {
        uint16_t x, y;
        // Sprawdź dotyk tylko jeśli nie jesteśmy w menu WiFi
        if (tft.getTouch(&x, &y)) { 
           Serial.println("🔍 Normal mode touch detected - resetting timer");
           lastMotionTime = millis(); // Resetuj uniwersalny timer aktywności
           
           if (currentDisplayState == DISPLAY_SLEEPING) {
               // Obudź ekran (chociaż PIR powinien to zrobić pierwszy)
               wakeUpDisplay(tft);
               currentDisplayState = DISPLAY_ACTIVE; // Ustaw stan ręcznie
           }
        }
    }
    
    // --- KROK 2: Sprawdź aktywność RUCHU (PIR) ---
    // Flaga motionDetected jest ustawiana przez przerwanie w handleMotionInterrupt()
    // handleMotionInterrupt() również resetuje lastMotionTime.
    if (motionDetected) {
        motionDetected = false; // Zresetuj flagę przerwania
        if (currentDisplayState == DISPLAY_SLEEPING) {
            wakeUpDisplay(tft);
            currentDisplayState = DISPLAY_ACTIVE; // Ustaw stan ręcznie
        }
        // Timer został już zresetowany w handleMotionInterrupt
    }

    // --- KROK 3: Główna logika stanów (Z DWOMA RÓŻNYMI TIMEOUTAMI) ---
    
    // Używamy lastMotionTime jako uniwersalnego timera "ostatniej aktywności"
    // (resetowanego przez PIR, dotyk WiFi lub dotyk normalny)
    
    // Ustal, którego timeoutu użyć na podstawie trybu
    unsigned long currentTimeout;
    if (isConfigModeActive) {
        // Jesteśmy w menu WiFi
        currentTimeout = MOTION_CONFIG_TIMEOUT; // 10 minut (600,000 ms)
    } else {
        // Jesteśmy w trybie normalnym (pogoda)
        currentTimeout = MOTION_TIMEOUT; // 60 sekund (60,000 ms) z pliku .h
    }
    
    unsigned long timeSinceLastActivity = millis() - lastMotionTime;

    switch (currentDisplayState) {
        case DISPLAY_ACTIVE: {
            // Sprawdź, czy minął czas bezczynności
            if (timeSinceLastActivity > currentTimeout) { // <-- Użyj dynamicznego timeoutu
                Serial.printf("💤 Timeout - przejście do DISPLAY_TIMEOUT (waited %lu ms) [Config: %s, Timeout: %lu ms]\n", 
                              timeSinceLastActivity, isConfigModeActive ? "YES" : "NO", currentTimeout);
                currentDisplayState = DISPLAY_TIMEOUT;
            }
            break;
        }
        case DISPLAY_TIMEOUT:
            // Stan pośredni, aby bezpiecznie wywołać uśpienie
            Serial.println("💤 Entering sleep mode");
            sleepDisplay(tft);
            currentDisplayState = DISPLAY_SLEEPING;
            break;
            
        case DISPLAY_SLEEPING:
            // Nie rób nic, czekaj na przerwanie PIR
            break;
    }
}
    
    /**
     * Budzi display (przy motion detection)
     */
    void wakeUpDisplay(TFT_eSPI& tft) {
        currentDisplayState = DISPLAY_ACTIVE;
        lastMotionTime = millis();
        
        // Clear screen
        tft.fillScreen(TFT_BLACK);
        
        // Show wake up message
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("WAKE UP!", tft.width() / 2, tft.height() / 2 - 30);
        tft.setTextSize(1);
        tft.drawString("Motion detected", tft.width() / 2, tft.height() / 2);
        tft.drawString("Starting weather station...", tft.width() / 2, tft.height() / 2 + 20);
        
        delay(DELAY_WAKE_UP_MESSAGE); // 2 sekundy na pokazanie wake up message
        
        Serial.println("✅ Display AWAKE - rozpoczynam stację pogodową");
    }
    
    /**
     * Usypia display i ESP32 (deep sleep)
     */
    void sleepDisplay(TFT_eSPI& tft) {
        currentDisplayState = DISPLAY_SLEEPING;
        
        // Pokaż sleep message
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("SLEEP MODE", tft.width() / 2, tft.height() / 2 - 20);
        tft.setTextSize(1);
        tft.drawString("Waiting for motion...", tft.width() / 2, tft.height() / 2 + 10);
        tft.drawString("PIR MOD-01655 active", tft.width() / 2, tft.height() / 2 + 30);
        tft.drawString("Deep sleep in 3s...", tft.width() / 2, tft.height() / 2 + 50);
        
        delay(DELAY_SLEEP_MESSAGE);
        
        // Wyłącz ekran całkowicie
        tft.fillScreen(TFT_BLACK);
        
        Serial.println("💤 ENTERING DEEP SLEEP - PIR wake up na GPIO " + String(PIR_PIN));
        Serial.flush(); // Upewnij się że komunikat zostanie wysłany
        
        // Konfiguruj PIR jako źródło wake up z deep sleep
        esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1); // Wake up when PIR goes HIGH
        
        // Wejście w deep sleep - ESP32 zatrzymuje się całkowicie
        esp_deep_sleep_start();
    }
    
    /**
     * Inicjalizuje PIR hardware
     */
    void initPIRHardware() {
        Serial.println("=== INICJALIZACJA PIR MOD-01655 ===");
        
        // Konfiguruj pin PIR jako input z pull-down
        pinMode(PIR_PIN, INPUT);
        
        // Konfiguruj wbudowaną diodę
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW);
        
        Serial.println("✅ PIR Sensor na GPIO " + String(PIR_PIN) + " gotowy!");
        Serial.println("💙 Wbudowana dioda na GPIO " + String(LED_BUILTIN) + " gotowa!");
        Serial.println("🕐 Timeout: " + String(MOTION_TIMEOUT/1000) + " sekund (1 minuta)");
    }
    
    // --- DEBUG ---
    void printDebugInfo() const {
        Serial.println("=== MotionSensorManager Debug ===");
        Serial.println("Motion Detected: " + String(motionDetected ? "YES" : "NO"));
        Serial.println("Display State: " + String(currentDisplayState));
        Serial.println("Last Motion: " + String(lastMotionTime) + " ms");
        Serial.println("Last Display Update: " + String(lastDisplayUpdate) + " ms");
        Serial.println("Motion Timeout: " + String(isMotionTimeout() ? "YES" : "NO"));
        Serial.println("Time since motion: " + String(millis() - lastMotionTime) + " ms");
    }
};

#endif