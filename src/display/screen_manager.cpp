#include "managers/ScreenManager.h"
#include "display/screen_manager.h"
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "display/github_image.h"
#include "config/display_config.h"
#include "sensors/motion_sensor.h"
#include "managers/WeatherCache.h"
#include "managers/TimeDisplayCache.h"

// Singleton instance ScreenManager
static ScreenManager screenManager;

ScreenManager& getScreenManager() {
  return screenManager;
}

// ❌ USUNIĘTE: 3 extern variables zastąpione ScreenManager class

void updateScreenManager() {
  // Nie przełączaj ekranów jeśli display śpi
  if (getDisplayState() == DISPLAY_SLEEPING) {
    return;
  }
  
  // Deleguj do ScreenManager - OOP style
  getScreenManager().updateScreenManager();
}

void switchToNextScreen(TFT_eSPI& tft) {
  // Deleguj do ScreenManager - OOP style
  getScreenManager().renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  // Deleguj do ScreenManager - OOP style
  getScreenManager().forceScreenRefresh(tft);
}

// --- IMPLEMENTACJA RENDERING METHODS dla ScreenManager ---

void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
  // Ekran 1: Aktualna pogoda + czas
  displayWeather(tft);
  displayTime(tft);
}

void ScreenManager::renderForecastScreen(TFT_eSPI& tft) {
  // Ekran 2: Prognoza 3h
  displayForecast(tft);
}

void ScreenManager::renderWeeklyScreen(TFT_eSPI& tft) {
  // Ekran 3: WEEKLY - prognoza 5-dniowa
  Serial.println("📱 Ekran wyczyszczony - rysowanie: WEEKLY");
  
  // Sprawdz czy dane weekly sa dostepne
  extern WeeklyForecastData weeklyForecast;
  
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    // Pokaz error screen
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("weekly forecast", 160, 110);
    tft.drawString("Wpisz 'weekly' w Serial", 160, 130);
    Serial.println("❌ Weekly data not available");
    return;
  }
  
  // === RENDERUJ DANE WEEKLY ===
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  int startY = 15;
  int rowHeight = 35;
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int y = startY + (i * rowHeight);
    
    // === NAZWA DNIA ===
    tft.setTextColor(TFT_WHITE); // Bialy
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // === IKONA POGODY (uproszczona kolorowa) ===
    int iconX = 55;
    int iconY = y + 5;
    
    // Rysuj uproszczona kolorowa ikone (lepsza czytelnosc)
    tft.fillCircle(iconX, iconY, 12, TFT_BLACK); // Tlo
    
    uint16_t iconColor = TFT_WHITE;
    if (day.icon.indexOf("01") >= 0) { 
      iconColor = 0xFFE0; // Zolte slonce
      tft.fillCircle(iconX, iconY, 8, iconColor);
      // Promienie slonca
      for(int i = 0; i < 8; i++) {
        int angle = i * 45;
        int x1 = iconX + cos(angle * PI/180) * 10;
        int y1 = iconY + sin(angle * PI/180) * 10;
        tft.drawPixel(x1, y1, iconColor);
      }
    }
    else if (day.icon.indexOf("02") >= 0 || day.icon.indexOf("03") >= 0) { 
      iconColor = 0xCE79; // Szare chmury
      tft.fillCircle(iconX-3, iconY, 6, iconColor);
      tft.fillCircle(iconX+3, iconY, 6, iconColor);
      tft.fillCircle(iconX, iconY-3, 6, iconColor);
    }
    else if (day.icon.indexOf("04") >= 0) { 
      iconColor = 0x7BEF; // Ciemne chmury
      tft.fillRect(iconX-8, iconY-4, 16, 8, iconColor);
    }
    else if (day.icon.indexOf("09") >= 0 || day.icon.indexOf("10") >= 0) { 
      iconColor = 0x07FF; // Cyan deszcz
      tft.fillRect(iconX-6, iconY-4, 12, 6, iconColor);
      // Krople
      for(int i = 0; i < 3; i++) {
        tft.drawLine(iconX-4+i*4, iconY+3, iconX-4+i*4, iconY+8, iconColor);
      }
    }
    else if (day.icon.indexOf("11") >= 0) { 
      iconColor = 0xF81F; // Magenta burza
      tft.fillRect(iconX-6, iconY-4, 12, 6, iconColor);
      // Blyskawiaca
      tft.drawLine(iconX-2, iconY-2, iconX+2, iconY+2, TFT_YELLOW);
      tft.drawLine(iconX+2, iconY-2, iconX-2, iconY+2, TFT_YELLOW);
    }
    else if (day.icon.indexOf("13") >= 0) { 
      iconColor = TFT_WHITE; // Bialy snieg
      tft.fillCircle(iconX-3, iconY-3, 3, iconColor);
      tft.fillCircle(iconX+3, iconY-3, 3, iconColor);
      tft.fillCircle(iconX, iconY+3, 3, iconColor);
    }
    else if (day.icon.indexOf("50") >= 0) { 
      iconColor = 0x8410; // Szara mgla
      for(int i = 0; i < 3; i++) {
        tft.drawLine(iconX-6, iconY-2+i*2, iconX+6, iconY-2+i*2, iconColor);
      }
    }
    
    // === TEMPERATURY MIN/MAX ===
    tft.setTextSize(2);
    
    // Min temp (szary)
    tft.setTextColor(TFT_LIGHTGREY); // Szary dla min
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String((int)day.tempMin) + "'", 120, y);
    
    // Separator
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", 155, y);
    
    // Max temp (bialy)
    tft.setTextColor(TFT_WHITE); // Bialy dla max
    tft.drawString(String((int)day.tempMax) + "'", 170, y);
    
    // === WIATR MIN/MAX ===
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    
    // Min wiatr (ciemnoszary)
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString(String((int)day.windMin) + "-", 220, y + 8);
    
    // Max wiatr (bialy)  
    tft.setTextColor(TFT_WHITE);
    tft.drawString(String((int)day.windMax) + "km/h", 240, y + 8);
    
    // === OPADY (zawsze pokazuj na ciemno niebiesko) ===
    tft.setTextColor(0x001F); // Ciemny niebieski
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String(day.precipitationChance) + "%", 285, y + 8);
    
    Serial.printf("📅 Rendered: %s, %.1f'-%.1f'C, %s\n", 
                  day.dayName.c_str(), day.tempMin, day.tempMax, day.icon.c_str());
  }
  
  // === FOOTER ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  unsigned long age = (millis() - weeklyForecast.lastUpdate) / 1000;
  tft.drawString("Update: " + String(age) + "s ago", 160, 210);
  
  Serial.println("✅ Weekly screen rendered with weather data");
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  // Ekran 3: Zdjęcie z GitHub
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  // Coordination z Phase 1+2 managers - teraz includes są w .cpp
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}