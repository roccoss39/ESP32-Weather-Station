#include "managers/ScreenManager.h"
#include "display/screen_manager.h"
#include "display/current_weather_display.h"
#include "display/forecast_display.h"
#include "display/weekly_forecast_display.h"
#include "display/sensors_display.h" // <--- NOWY INCLUDE
#include "display/time_display.h"
#include "display/github_image.h"
#include "config/display_config.h"
#include "sensors/motion_sensor.h"
#include "sensors/dht22_sensor.h"
#include "managers/WeatherCache.h"
#include "managers/TimeDisplayCache.h"
#include "config/location_config.h"
#include "config/hardware_config.h" 

// --- EXTERNAL DEPENDENCIES ---
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;

// Singleton instance ScreenManager
static ScreenManager screenManager;

ScreenManager& getScreenManager() {
  return screenManager;
}

void updateScreenManager() {
  if (getDisplayState() == DISPLAY_SLEEPING) {
    return;
  }
  getScreenManager().updateScreenManager();
}

// --- ZMIENNE GLOBALNE I EXTERN ---
extern bool isOfflineMode;
extern void drawNASAImage(TFT_eSPI& tft, bool forceFallback); 
extern void displaySystemStatus(TFT_eSPI& tft);


// ================================================================
// GŁÓWNA FUNKCJA PRZEŁĄCZANIA EKRANÓW (LOGIKA OFFLINE/ONLINE)
// ================================================================
void switchToNextScreen(TFT_eSPI& tft) {
    ScreenManager& mgr = getScreenManager();

    // === 1. TRYB OFFLINE (BEZ WIFI) ===
    if (isOfflineMode) {
        
        ScreenType originalScreen = mgr.getCurrentScreen(); 
        
        // Parzyste = Sensory (trwa 2x dłużej), Nieparzyste = Obrazek
        if ((int)originalScreen % 2 == 0) {
            // --- A. RYSOWANIE SENSORÓW (Z UŻYCIEM NOWEGO PLIKU) ---
            mgr.setCurrentScreen(SCREEN_LOCAL_SENSORS); 
            // mgr.renderCurrentScreen(tft) wywoła odpowiednią funkcję
            // ale musimy ręcznie wywołać, bo nadpisujemy logikę:
            displayLocalSensors(tft); // <--- BEZPOŚREDNIE WYWOŁANIE
            
            // Zegar na samej górze
            displayTime(tft); 
            
        } else {
            // --- B. RYSOWANIE OBRAZKA ---
            mgr.setCurrentScreen(SCREEN_IMAGE);         
            drawNASAImage(tft, true); // True = Force Fallback (z pamięci)
            
            // Info o galerii offline na dole obrazka
            tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_BLACK);
            tft.setTextSize(1);
            tft.setTextDatum(BC_DATUM);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
            tft.drawString("GALERIA OFFLINE", tft.width() / 2, tft.height() - 5);
        }
        
        mgr.setCurrentScreen(originalScreen); // Przywróć licznik dla timera
        return;
    }

    // === 2. TRYB ONLINE (NORMALNY) ===
    mgr.renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  getScreenManager().forceScreenRefresh(tft);
}

// ================================================================
// IMPLEMENTACJA RENDERING METHODS dla ScreenManager
// ================================================================

void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
    tft.fillScreen(COLOR_BACKGROUND);
    displayTime(tft);
    displayCurrentWeather(tft);  
}

void ScreenManager::renderForecastScreen(TFT_eSPI& tft) {
  displayForecast(tft);
}

void ScreenManager::renderWeeklyScreen(TFT_eSPI& tft) {
  displayWeeklyForecast(tft);  
}

// === EKRAN 4: LOCAL SENSORS ===
void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
  // Przekazujemy sterowanie do nowego, dedykowanego pliku
  displayLocalSensors(tft);
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}