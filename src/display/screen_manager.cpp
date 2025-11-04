#include "display/screen_manager.h"
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "config/display_config.h"

// Definicje zmiennych globalnych
ScreenType currentScreen = SCREEN_CURRENT_WEATHER;
unsigned long lastScreenSwitch = 0;
const unsigned long SCREEN_SWITCH_INTERVAL = 5000; // 5 sekund

void updateScreenManager() {
  unsigned long currentTime = millis();
  
  // Sprawdź czy czas na przełączenie ekranu
  if (currentTime - lastScreenSwitch >= SCREEN_SWITCH_INTERVAL) {
    lastScreenSwitch = currentTime;
    
    // Przełącz na następny ekran
    if (currentScreen == SCREEN_CURRENT_WEATHER) {
      currentScreen = SCREEN_FORECAST;
      Serial.println("Przełączanie na ekran PROGNOZY");
    } else {
      currentScreen = SCREEN_CURRENT_WEATHER;
      Serial.println("Przełączanie na ekran AKTUALNEJ POGODY");
    }
  }
}

void switchToNextScreen(TFT_eSPI& tft) {
  // ZAWSZE wyczyść cały ekran przed przełączeniem
  tft.fillScreen(COLOR_BACKGROUND);
  
  Serial.println("Ekran wyczyszczony - rysowanie nowego zawartosci");
  
  if (currentScreen == SCREEN_CURRENT_WEATHER) {
    // Ekran 1: Aktualna pogoda + czas
    
    // RESET CACHE POGODY - wymusza ponowne rysowanie
    extern float weatherCachePrev_temperature;
    extern float weatherCachePrev_feelsLike;
    extern float weatherCachePrev_humidity;
    extern float weatherCachePrev_windSpeed;
    extern float weatherCachePrev_pressure;
    extern String weatherCachePrev_description;
    extern String weatherCachePrev_icon;
    
    weatherCachePrev_temperature = -999.0;  // Reset cache
    weatherCachePrev_feelsLike = -999.0;    // Reset cache
    weatherCachePrev_humidity = -999.0;     // Reset cache
    weatherCachePrev_windSpeed = -999.0;    // Reset cache
    weatherCachePrev_pressure = -999.0;     // Reset cache
    weatherCachePrev_description = "";      // Reset cache
    weatherCachePrev_icon = "";             // Reset cache
    
    Serial.println("DEBUG: Reset cache pogody - wymuszam rysowanie");
    
    displayWeather(tft);
    
    // Wymuś pokazanie wszystkich elementów czasu natychmiast
    extern String dayStrPrev;
    extern char timeStrPrev[9];
    extern char dateStrPrev[11];
    extern int wifiStatusPrev;
    
    dayStrPrev = "";           // Reset cache
    strcpy(timeStrPrev, "");   // Reset cache  
    strcpy(dateStrPrev, "");   // Reset cache
    wifiStatusPrev = -1;       // Reset cache
    
    displayTime(tft);          // Wywołaj po reset cache - elementy się pokażą od razu
    
  } else if (currentScreen == SCREEN_FORECAST) {
    // Ekran 2: Prognoza 3h
    displayForecast(tft);
  }
}

void forceScreenRefresh(TFT_eSPI& tft) {
  // Wymusza odświeżenie aktualnego ekranu
  switchToNextScreen(tft);
}