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

// --- EXTERNAL DEPENDENCIES ---
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;
extern bool isOfflineMode;
// Te funkcje muszą być dostępne:
extern void drawNASAImage(TFT_eSPI& tft, bool forceFallback); 
extern void displaySystemStatus(TFT_eSPI& tft);
extern void displayLocalSensors(TFT_eSPI& tft); // Zakładam, że to jest w display/sensors_display.cpp

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
    // W starej wersji tu była logika. Teraz logika jest w klasie.
    // Po prostu każemy Managerowi narysować to, co aktualnie ma ustawione.
    getScreenManager().renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  getScreenManager().forceScreenRefresh(tft);
}

// ================================================================
// IMPLEMENTACJA METOD KLASY ScreenManager
// ================================================================

void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
    // Wyczyść jest już w renderCurrentScreen w .h, ale dla pewności tło pogody:
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
    // 1. Wyświetl sensory (zewnętrzna funkcja)
    // Upewnij się w sensors_display.cpp, że rysują się niżej (y > 70)
    displayLocalSensors(tft);
    
    // 2. Wyświetl Zegar
    displayTime(tft); 

    // 3. --- DATA (TWOJA RAMKA - PRZENIESIONA TUTAJ) ---
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        uint16_t CARD_BG = 0x1082; 
        uint16_t BORDER_COLOR = TFT_DARKGREY;
        int cardW = 160; 
        int cardH = 30;
        int cardX = (tft.width() - cardW) / 2; 
        int cardY = 35;

        tft.fillRoundRect(cardX, cardY, cardW, cardH, 6, CARD_BG);
        tft.drawRoundRect(cardX, cardY, cardW, cardH, 6, BORDER_COLOR);

        char dateStr[16];
        sprintf(dateStr, "%02d.%02d.%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        // Uwaga: const char* tablica powinna być static dla oszczędności, ale tak też zadziała
        const char* daysPL[] = {"NIEDZIELA", "PONIEDZIALEK", "WTOREK", "SRODA", "CZWARTEK", "PIATEK", "SOBOTA"};
        
        tft.setTextColor(TFT_SILVER, CARD_BG);
        tft.setTextSize(1);
        tft.setTextDatum(ML_DATUM);
        
        // Zabezpieczenie przed wyjściem poza tablicę (0-6)
        if(timeinfo.tm_wday >= 0 && timeinfo.tm_wday <= 6) {
            tft.drawString(daysPL[timeinfo.tm_wday], cardX + 10, cardY + cardH/2);
        }
        
        tft.setTextColor(TFT_WHITE, CARD_BG);
        tft.setTextDatum(MR_DATUM);
        tft.drawString(dateStr, cardX + cardW - 10, cardY + cardH/2);
    }
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
      tft.drawString("GALERIA OFFLINE", tft.width() / 2, tft.height() - 5);
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