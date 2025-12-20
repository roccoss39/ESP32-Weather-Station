#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#define TEST_MODE 0
#define SCREEN_SWITCH_INTERVAL_ 2000

extern bool isOfflineMode;

// ScreenType enum definition - musi być tutaj dla ScreenManager
enum ScreenType {
  SCREEN_CURRENT_WEATHER = 0,
  SCREEN_FORECAST = 1,
  SCREEN_WEEKLY = 2,
  SCREEN_LOCAL_SENSORS = 3,
  SCREEN_IMAGE = 4
};

class ScreenManager {
private:
    // --- PRIVATE STATE ---
    ScreenType currentScreen = SCREEN_CURRENT_WEATHER;
    unsigned long lastScreenSwitch = 0;
    static const unsigned long SCREEN_SWITCH_INTERVAL = SCREEN_SWITCH_INTERVAL_; // 10 sekund

public:
    // --- CONSTRUCTOR ---
    ScreenManager() {
        lastScreenSwitch = millis();
        Serial.println("📱 ScreenManager initialized - starting with WEATHER screen");
    }
    
    // --- GETTERS ---
    ScreenType getCurrentScreen() const { return currentScreen; }
    unsigned long getLastSwitch() const { return lastScreenSwitch; }
    unsigned long getSwitchInterval() const { return SCREEN_SWITCH_INTERVAL; }
    
    // --- SETTERS ---
    void setCurrentScreen(ScreenType screen) { 
        currentScreen = screen; 
        lastScreenSwitch = millis();
    }
    
    /**
     * Resetuje timer rotacji ekranów - przydatne po reconnect WiFi
     * Zapewnia pełny 60s cykl przed sleep mode
     */
    void resetScreenTimer() {
        lastScreenSwitch = millis();
        Serial.println("📱 Screen timer RESET - full 60s cycle before sleep mode");
    }
    
    // --- BUSINESS LOGIC ---
    
    /**
     * Sprawdza czy czas na przełączenie ekranu
     * @return true jeśli minął SCREEN_SWITCH_INTERVAL
     */
    bool shouldSwitchScreen() const {
        unsigned long interval = SCREEN_SWITCH_INTERVAL;

        // Jeśli jesteśmy OFFLINE i wyświetlamy SENSORY (czyli parzysty numer ekranu w naszej sztuczce)
        if (isOfflineMode && ((int)currentScreen % 2 == 0)) {
             // Ustawiamy czas 2x dłuższy (np. 20 sekund zamiast 10)
             interval = SCREEN_SWITCH_INTERVAL * 2;
        }

        return (millis() - lastScreenSwitch) >= interval;
    }
    
    /**
     * Przełącza na następny ekran w rotacji
     * @return poprzedni ekran (dla logowania)
     */
    ScreenType switchToNext() {
        ScreenType previousScreen = currentScreen;
        lastScreenSwitch = millis();

        if (TEST_MODE == 1) {
            static int nr = 0;
            nr++;
            if(nr%2)
            currentScreen = SCREEN_FORECAST;  // W trybie testowym tylko IMAGE
            else 
            currentScreen = SCREEN_CURRENT_WEATHER;
        }

        switch(currentScreen) {
            case SCREEN_CURRENT_WEATHER:
                currentScreen = SCREEN_FORECAST;
                Serial.println("📱 Przełączanie: WEATHER → FORECAST");
                break;
            case SCREEN_FORECAST:
                currentScreen = SCREEN_WEEKLY;
                Serial.println("📱 Przełączanie: FORECAST → WEEKLY");
                break;
            case SCREEN_WEEKLY:
                currentScreen = SCREEN_LOCAL_SENSORS;
                Serial.println("📱 Przełączanie: WEEKLY → LOCAL_SENSORS");
                break;
            case SCREEN_LOCAL_SENSORS:
                currentScreen = SCREEN_IMAGE;
                Serial.println("📱 Przełączanie: LOCAL_SENSORS → IMAGE");
                break;
            case SCREEN_IMAGE:
                currentScreen = SCREEN_CURRENT_WEATHER;
                Serial.println("📱 Przełączanie: IMAGE → WEATHER");
                break;
        }
        
        return previousScreen;
    }
    
    /**
     * Główna logika zarządzania ekranami
     * Wywołuj w każdym loop() - sprawdza timing i przełącza ekrany
     */
    void updateScreenManager() {
        if (!shouldSwitchScreen()) {
            return; // Jeszcze nie czas na przełączenie
        }
        
        // Przełącz na następny ekran
        switchToNext();
    }
    
    /**
     * Renderuje aktualny ekran z proper cache management
     * @param tft TFT display reference
     */
    void renderCurrentScreen(TFT_eSPI& tft) {
        // ZAWSZE wyczyść cały ekran przed przełączeniem
        tft.fillScreen(TFT_BLACK); // Używamy TFT_BLACK zamiast COLOR_BACKGROUND dla pewności
        
        Serial.println("📱 Ekran wyczyszczony - rysowanie: " + getScreenName(currentScreen));
        
        // Reset cache dla aktualnego ekranu (coordination z Phase 1+2)
        resetCacheForScreen(currentScreen);
        
        // Renderuj odpowiedni ekran
        // if (TEST_MODE == 1) {
        //     static int nr = 0;
        //     nr++;
        //     if(nr%2)
        //     currentScreen = SCREEN_IMAGE;  // W trybie testowym tylko IMAGE
        //     else 
        //     currentScreen = SCREEN_CURRENT_WEATHER;
        // }

        switch(currentScreen) {
            case SCREEN_CURRENT_WEATHER:
                renderWeatherScreen(tft);
                break;
            case SCREEN_FORECAST:
                renderForecastScreen(tft);
                break;
            case SCREEN_WEEKLY:
                renderWeeklyScreen(tft);
                break;
            case SCREEN_LOCAL_SENSORS:
                renderLocalSensorsScreen(tft);
                break;
            case SCREEN_IMAGE:
                renderImageScreen(tft);
                break;
        }
    }
    
    /**
     * Wymusza odświeżenie aktualnego ekranu
     * @param tft TFT display reference
     */
    void forceScreenRefresh(TFT_eSPI& tft) {
        Serial.println("📱 FORCE REFRESH: " + getScreenName(currentScreen));
        renderCurrentScreen(tft);
    }
    
    /**
     * Przełącza na konkretny ekran (nie w kolejności)
     * @param screen Docelowy ekran
     * @param tft TFT display reference
     */
    void switchToScreen(ScreenType screen, TFT_eSPI& tft) {
        if (currentScreen == screen) {
            return; // Już jesteśmy na tym ekranie
        }
        
        Serial.println("📱 MANUAL SWITCH: " + getScreenName(currentScreen) + " → " + getScreenName(screen));
        currentScreen = screen;
        lastScreenSwitch = millis();
        renderCurrentScreen(tft);
    }
    
    // --- HELPER METHODS ---
    
    /**
     * Pobiera nazwę ekranu do debugowania
     * @param screen Typ ekranu
     * @return Nazwa jako String
     */
    String getScreenName(ScreenType screen) const {
        switch(screen) {
            case SCREEN_CURRENT_WEATHER: return "WEATHER";
            case SCREEN_FORECAST: return "FORECAST";
            case SCREEN_WEEKLY: return "WEEKLY";
            case SCREEN_LOCAL_SENSORS: return "LOCAL_SENSORS";
            case SCREEN_IMAGE: return "IMAGE";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * Resetuje cache dla konkretnego ekranu
     * Coordination z WeatherCache i TimeDisplayCache
     * @param screen Typ ekranu
     */
    void resetCacheForScreen(ScreenType screen) {
        switch(screen) {
            case SCREEN_CURRENT_WEATHER:
                // Reset cache pogody + czasu (Phase 1+2 coordination)
                resetWeatherAndTimeCache();
                Serial.println("📱 Reset cache: WEATHER + TIME");
                break;
            case SCREEN_FORECAST:
                // Reset tylko cache prognozy
                Serial.println("📱 Reset cache: FORECAST");
                break;
            case SCREEN_WEEKLY:
                // Reset cache weekly
                Serial.println("📱 Reset cache: WEEKLY");
                break;
            case SCREEN_IMAGE:
                // Nie ma cache dla obrazów
                Serial.println("📱 Reset cache: IMAGE (none)");
                break;
        }
    }
    
    // --- RENDERING METHODS (do implementacji w .cpp) ---
    void renderWeatherScreen(TFT_eSPI& tft);
    void renderForecastScreen(TFT_eSPI& tft);
    void renderWeeklyScreen(TFT_eSPI& tft);
    void renderLocalSensorsScreen(TFT_eSPI& tft);
    void renderImageScreen(TFT_eSPI& tft);
    void resetWeatherAndTimeCache();
    
    // --- DEBUG ---
    void printDebugInfo() const {
        Serial.println("=== ScreenManager Debug ===");
        Serial.println("Current Screen: " + getScreenName(currentScreen) + " (" + String((int)currentScreen) + ")");
        Serial.println("Last Switch: " + String(lastScreenSwitch) + " ms");
        Serial.println("Time since switch: " + String(millis() - lastScreenSwitch) + " ms");
        Serial.println("Switch interval: " + String(SCREEN_SWITCH_INTERVAL) + " ms");
        Serial.println("Should switch: " + String(shouldSwitchScreen() ? "YES" : "NO"));
    }
    
    // --- TIMING HELPERS ---
    
    /**
     * Pobiera pozostały czas do przełączenia ekranu
     * @return czas w ms (0 jeśli czas minął)
     */
    unsigned long getTimeUntilSwitch() const {
        unsigned long elapsed = millis() - lastScreenSwitch;
        if (elapsed >= SCREEN_SWITCH_INTERVAL) {
            return 0;
        }
        return SCREEN_SWITCH_INTERVAL - elapsed;
    }
    
    /**
     * Pobiera procent czasu od ostatniego przełączenia
     * @return 0-100 procent
     */
    int getSwitchProgress() const {
        unsigned long elapsed = millis() - lastScreenSwitch;
        if (elapsed >= SCREEN_SWITCH_INTERVAL) {
            return 100;
        }
        return (elapsed * 100) / SCREEN_SWITCH_INTERVAL;
    }
};

#endif