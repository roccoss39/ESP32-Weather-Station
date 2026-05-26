#include "managers/ScreenManager.h"
#include "display/screen_manager.h" // Upewnij się, że nie dublujesz includów
#include "display/current_weather_display.h"
#include "display/forecast_display.h"
#include "display/weekly_forecast_display.h"
#include "display/sensors_display.h"
#include "display/time_display.h"
#include "display/github_image.h"
#include "config/display_config.h"
#include "sensors/motion_sensor.h"
#include "sensors/dht22_sensor.h"
#include "managers/WeatherCache.h"
#include "managers/TimeDisplayCache.h"
#include "config/location_config.h"
#include "config/hardware_config.h" 

extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;
extern bool isOfflineMode;

extern void drawNASAImage(TFT_eSPI& tft, bool forceFallback); 
extern void displaySystemStatus(TFT_eSPI& tft);

// Singleton instance ScreenManager
static ScreenManager screenManager;

ScreenManager& getScreenManager() {
  return screenManager;
}

void updateScreenManager() {
  if (getDisplayState() == DISPLAY_SLEEPING) {
    return;
  }
  // To wywoła metodę z klasy, która sprawdzi czas i zmieni currentScreen jeśli trzeba
  getScreenManager().updateScreenManager();
}


// ================================================================
// GŁÓWNA FUNKCJA RENDERUJĄCA (GLOBAL WRAPPER)
// ================================================================
void switchToNextScreen(TFT_eSPI& tft) {
    getScreenManager().renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  getScreenManager().forceScreenRefresh(tft);
}

// ================================================================
// IMPLEMENTACJA METOD KLASY ScreenManager
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

// === EKRAN 4: LOCAL SENSORS (Tu trafi Twój kod daty!) ===
void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
   if (isOfflineMode) {
    displayLocalSensors(tft);
  }
  else 
  displayLocalSensors(tft);
    // ----------------------------------------------------
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  if (isOfflineMode) {
      // W trybie offline wymuszamy obrazek z pamięci (fallback)
      drawNASAImage(tft, true); 
      
      // Info o galerii offline
      tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextDatum(BC_DATUM);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
      tft.drawString(" ", tft.width() / 2, tft.height() - 5);
  } else {
      // W trybie online normalnie
      displayGitHubImage(tft);
  }
}

void ScreenManager::resetWeatherAndTimeCache() {
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}