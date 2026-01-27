#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config/timing_config.h" 

#define TEST_MODE 0
#define TEST_SCREEN SCREEN_WEEKLY

extern bool isOfflineMode;


enum ScreenType {
  SCREEN_CURRENT_WEATHER = 0,
  SCREEN_FORECAST = 1,
  SCREEN_WEEKLY = 2,
  SCREEN_LOCAL_SENSORS = 3,
  SCREEN_IMAGE = 4
};

class ScreenManager {
private:
    ScreenType currentScreen = SCREEN_CURRENT_WEATHER;
    unsigned long lastScreenSwitch = 0;
    static const unsigned long SCREEN_SWITCH_INTERVAL = SCREEN_SWITCH_INTERVAL_; 

public:
    ScreenManager() {
        lastScreenSwitch = millis();
        Serial.println("📱 ScreenManager initialized");
    }
    
    // --- GETTERS ---
    ScreenType getCurrentScreen() const { return currentScreen; }
    unsigned long getLastSwitch() const { return lastScreenSwitch; }
    unsigned long getSwitchInterval() const { return SCREEN_SWITCH_INTERVAL; }
    
    // --- SETTERS (THE BOUNCER / BRAMKARZ) ---
    
    void setCurrentScreen(ScreenType screen) { 
        if (isOfflineMode) {
            // W trybie Offline pozwalamy tylko na SENSORY lub OBRAZEK
            if (screen == SCREEN_LOCAL_SENSORS || screen == SCREEN_IMAGE) {
                currentScreen = screen;
            } else {
                // Każda inna prośba (pogoda) przekierowywana na sensory
                Serial.println("⛔ Offline Mode: Wymuszono SENSORS zamiast ekranu internetowego");
                currentScreen = SCREEN_LOCAL_SENSORS;
            }
        } else {
            currentScreen = screen; 
        }
        lastScreenSwitch = millis();
    }
    
    void resetScreenTimer() {
        lastScreenSwitch = millis();
        Serial.println("📱 Screen timer RESET");
    }
    
    // --- BUSINESS LOGIC ---
    
    /**
     * Sprawdza czy czas na przełączenie ekranu
     * @return true jeśli minął wyznaczony czas
     */
    bool shouldSwitchScreen() const {
        unsigned long interval = SCREEN_SWITCH_INTERVAL; // Domyślnie 10s

        if (isOfflineMode) {
            // === LOGIKA CZASU DLA OFFLINE ===
            
            if (currentScreen == SCREEN_LOCAL_SENSORS) {
                // Jeśli wyświetlamy SENSORY -> trzymamy je 2x dłużej (np. 20s)
                // Żeby zdążyć przeczytać godzinę, datę i temperaturę
                interval = SCREEN_SWITCH_INTERVAL * 2;
            } 
            else {
                // Jeśli wyświetlamy OBRAZEK -> standardowy czas (np. 10s)
                // Żeby tylko rzucić okiem na galerię
                interval = SCREEN_SWITCH_INTERVAL;
            }
        }

        return (millis() - lastScreenSwitch) >= interval;
    }
    
    /**
     * Przełącza na następny ekran w rotacji
     */
    ScreenType switchToNext() {
        ScreenType previousScreen = currentScreen;
        lastScreenSwitch = millis();

        // 1. LOGIKA DLA TRYBU OFFLINE (Pętla: Sensory <-> Obrazek)
        if (isOfflineMode) {
            if (currentScreen == SCREEN_LOCAL_SENSORS) {
                // Jeśli są sensory -> idź do obrazka
                currentScreen = SCREEN_IMAGE;
                Serial.println("📱 Offline Rotacja: SENSORS → IMAGE");
            } 
            else if (currentScreen == SCREEN_IMAGE) {
                // Jeśli jest obrazek -> wróć do sensorów
                currentScreen = SCREEN_LOCAL_SENSORS;
                Serial.println("📱 Offline Rotacja: IMAGE → SENSORS");
            } 
            else {
                // Jeśli jesteśmy na jakimś "zakazanym" ekranie (np. tuż po utracie WiFi)
                // Ustawiamy bezpieczny start
                currentScreen = SCREEN_LOCAL_SENSORS;
                Serial.println("📱 Offline Start: → SENSORS");
            }
            return previousScreen;
        }

        // 2. TRYB TESTOWY
        if (TEST_MODE == 1) {
            currentScreen = TEST_SCREEN; 
            return previousScreen;
        }

        // 3. NORMALNA ROTACJA ONLINE (Pełna pętla)
        switch(currentScreen) {
            case SCREEN_CURRENT_WEATHER:
                currentScreen = SCREEN_FORECAST;
                break;
            case SCREEN_FORECAST:
                currentScreen = SCREEN_WEEKLY;
                break;
            case SCREEN_WEEKLY:
                currentScreen = SCREEN_LOCAL_SENSORS;
                break;
            case SCREEN_LOCAL_SENSORS:
                currentScreen = SCREEN_IMAGE;
                break;
            case SCREEN_IMAGE:
                currentScreen = SCREEN_CURRENT_WEATHER;
                break;
            default:
                currentScreen = SCREEN_CURRENT_WEATHER;
                break;
        }
        
        return previousScreen;
    }
    
    void updateScreenManager() {
        if (!shouldSwitchScreen()) {
            return; 
        }
        switchToNext();
    }
    
    void renderCurrentScreen(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK); 
        resetCacheForScreen(currentScreen);
        
        // Zabezpieczenie przed renderowaniem "zakazanych" ekranów w offline
        if (isOfflineMode) {
             if (currentScreen != SCREEN_LOCAL_SENSORS && currentScreen != SCREEN_IMAGE) {
                 currentScreen = SCREEN_LOCAL_SENSORS;
             }
        }

        if (TEST_MODE == 1) currentScreen = TEST_SCREEN;

        switch(currentScreen) {
            case SCREEN_CURRENT_WEATHER: renderWeatherScreen(tft); break;
            case SCREEN_FORECAST:        renderForecastScreen(tft); break;
            case SCREEN_WEEKLY:          renderWeeklyScreen(tft); break;
            case SCREEN_LOCAL_SENSORS:   renderLocalSensorsScreen(tft); break;
            case SCREEN_IMAGE:           renderImageScreen(tft); break;
        }
    }
    
    void forceScreenRefresh(TFT_eSPI& tft) {
        renderCurrentScreen(tft);
    }
    
    void switchToScreen(ScreenType screen, TFT_eSPI& tft) {
        // Zabezpieczenie manualnego przełączania w offline
        if (isOfflineMode) {
            if (screen != SCREEN_LOCAL_SENSORS && screen != SCREEN_IMAGE) {
                 Serial.println("⛔ Offline Mode: Próba wejścia na ekran internetowy zablokowana.");
                 return; // Po prostu nic nie rób, zostań gdzie jesteś
            }
        }

        if (currentScreen == screen) return;
        
        currentScreen = screen;
        lastScreenSwitch = millis();
        renderCurrentScreen(tft);
    }
    
    // --- HELPER METHODS ---
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
    
    void resetCacheForScreen(ScreenType screen) {
        switch(screen) {
            case SCREEN_CURRENT_WEATHER: resetWeatherAndTimeCache(); break;
            case SCREEN_FORECAST: break;
            case SCREEN_WEEKLY: break;
            case SCREEN_IMAGE: break;
        }
    }
    
    // --- EXTERNAL METHODS ---
    void renderWeatherScreen(TFT_eSPI& tft);
    void renderForecastScreen(TFT_eSPI& tft);
    void renderWeeklyScreen(TFT_eSPI& tft);
    void renderLocalSensorsScreen(TFT_eSPI& tft);
    void renderImageScreen(TFT_eSPI& tft);
    void resetWeatherAndTimeCache();
};

void updateScreenManager();
void switchToNextScreen(TFT_eSPI& tft);
ScreenManager& getScreenManager();
void forceScreenRefresh(TFT_eSPI& tft);

#endif