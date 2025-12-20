#include "managers/ScreenManager.h"
#include "display/screen_manager.h"
#include "display/weather_display.h"
#include "display/forecast_display.h"
#include "display/time_display.h"
#include "display/github_image.h"
#include "config/display_config.h"
#include "sensors/motion_sensor.h"
#include "sensors/dht22_sensor.h"
#include "managers/WeatherCache.h"
#include "managers/TimeDisplayCache.h"
#include "config/location_config.h"

// --- EXTERNAL DEPENDENCIES ---
extern WeeklyForecastData weeklyForecast;
extern unsigned long lastWeatherCheckGlobal;

// Funkcja z weather_display.cpp
extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);

// --- FALLBACK CONSTANTS ---
#ifndef UPDATES_TITLE_Y
  #define UPDATES_CLEAR_Y   130
  #define UPDATES_TITLE_Y   140
  #define UPDATES_DHT22_Y   160
  #define UPDATES_SENSOR_Y  175
  #define UPDATES_WEATHER_Y 190
  #define UPDATES_WEEKLY_Y  205
  #define UPDATES_WIFI_Y    220
#endif

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

extern bool isOfflineMode;
extern void drawNASAImage(TFT_eSPI& tft, bool forceFallback); 
extern void displaySystemStatus(TFT_eSPI& tft);


void switchToNextScreen(TFT_eSPI& tft) {
    ScreenManager& mgr = getScreenManager();

    // === 1. TRYB OFFLINE (BEZ WIFI) ===
    if (isOfflineMode) {
        
        ScreenType originalScreen = mgr.getCurrentScreen(); 
        
        // Parzyste = Sensory (2x dłużej), Nieparzyste = Obrazek
        if ((int)originalScreen % 2 == 0) {
            // === RYSOWANIE SENSORÓW ===
            mgr.setCurrentScreen(SCREEN_LOCAL_SENSORS); 
            mgr.renderCurrentScreen(tft);               
            
            displayTime(tft); // Narysuj zegar
            
            // ZAMAZANIE STOPKI AKTUALIZACJI POGODY
            // Rysujemy czarny pasek na samym dole (ostatnie 20 pikseli), 
            // żeby zakryć tekst typu "Zaktualizowano: 12:30"
            tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_BLACK);
            
            // Rysujemy napis OFFLINE w tym miejscu (ZAMIAST daty aktualizacji)
            tft.setTextSize(1);
            tft.setTextDatum(BC_DATUM); // Bottom Center (Dół Środek)
            tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
            tft.drawString("TRYB OFFLINE - SENSORY LOKALNE", tft.width() / 2, tft.height() - 5);

        } else {
            // === RYSOWANIE OBRAZKA ===
            mgr.setCurrentScreen(SCREEN_IMAGE);         
            drawNASAImage(tft, true);                   
            
            // Na obrazku też dajemy info na dole (opcjonalnie)
            tft.fillRect(0, tft.height() - 20, tft.width(), 20, TFT_BLACK);
            tft.setTextSize(1);
            tft.setTextDatum(BC_DATUM);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
            tft.drawString("TRYB OFFLINE - GALERIA", tft.width() / 2, tft.height() - 5);
        }
        
        mgr.setCurrentScreen(originalScreen); // Przywróć licznik
        return;
    }

    // === 2. TRYB ONLINE (NORMALNY) ===
    mgr.renderCurrentScreen(tft);
}

void forceScreenRefresh(TFT_eSPI& tft) {
  getScreenManager().forceScreenRefresh(tft);
}

// --- IMPLEMENTACJA RENDERING METHODS dla ScreenManager ---

void ScreenManager::renderWeatherScreen(TFT_eSPI& tft) {
  displayWeather(tft);
  displayTime(tft);
}

void ScreenManager::renderForecastScreen(TFT_eSPI& tft) {
  displayForecast(tft);
}

void ScreenManager::renderWeeklyScreen(TFT_eSPI& tft) {
  // Ekran 3: WEEKLY - prognoza 5-dniowa
  Serial.println("📱 Ekran wyczyszczony - rysowanie: WEEKLY");
  
  unsigned long dataAge = millis() - weeklyForecast.lastUpdate;
  Serial.printf("📊 Weekly data: valid=%s, count=%d, age=%.1fh\n", 
               weeklyForecast.isValid ? "YES" : "NO", weeklyForecast.count, dataAge / 3600000.0);
  
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Brak danych", 160, 80);
    tft.setTextSize(1);
    tft.drawString("prognoza tygodniowa", 160, 110);
    tft.drawString("Wpisz 'x' w Serial", 160, 130);
    Serial.println("❌ Weekly data not available");
    return;
  }
  
  // === RENDERUJ DANE WEEKLY ===
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  int availableHeight = 200; 
  int startY = 15;
  int rowHeight = weeklyForecast.count > 0 ? (availableHeight / weeklyForecast.count) : 35;
  
  int textOffset = (rowHeight - 16) / 2; 
  if (textOffset < 0) textOffset = 0;

  Serial.printf("📐 Wyrównanie: %d dni, wys. rzędu: %dpx, offset: %dpx\n", 
                weeklyForecast.count, rowHeight, textOffset);
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int rawY = startY + (i * rowHeight);
    int y = rawY + textOffset; 
    
    // === NAZWA DNIA ===
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // === PRAWDZIWE IKONY POGODOWE ===
    int iconX = 55;
    int iconY = rawY - (rowHeight / 4) + textOffset;
    
    String condition = "";
    if (day.icon.indexOf("01") >= 0) condition = "clear sky";
    else if (day.icon.indexOf("02") >= 0) condition = "few clouds";
    else if (day.icon.indexOf("03") >= 0) condition = "scattered clouds";
    else if (day.icon.indexOf("04") >= 0) condition = "overcast clouds";
    else if (day.icon.indexOf("09") >= 0) condition = "shower rain";
    else if (day.icon.indexOf("10") >= 0) condition = "rain";
    else if (day.icon.indexOf("11") >= 0) condition = "thunderstorm";
    else if (day.icon.indexOf("13") >= 0) condition = "snow";
    else if (day.icon.indexOf("50") >= 0) condition = "mist";
    else condition = "unknown";
    
    drawWeatherIcon(tft, iconX, iconY, condition, day.icon);
    
    // === TEMPERATURY MIN/MAX ===
    tft.setTextSize(2);
    
    tft.setTextColor(TFT_DARKGREY); 
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String((int)round(day.tempMin)) + "'", 120, y);
    
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", 155, y);
    
    tft.setTextColor(TFT_WHITE); 
    tft.drawString(String((int)round(day.tempMax)) + "'", 170, y);
    
    // ================================================================
    // LOGIKA AUTO-SCALING (Wiatr i Opady)
    // ================================================================
    
    int tMinInt = (int)round(day.tempMin);
    int tMaxInt = (int)round(day.tempMax);

    bool tempsBothNegative = (tMinInt < 0 && tMaxInt < 0);
    bool tempsBothDouble = (abs(tMinInt) >= 10 && abs(tMaxInt) >= 10);
    bool dataCrowded = (day.windMin >= 10 && day.windMax >= 10 && day.precipitationChance >= 10);
    bool dataHuge = (day.windMin >= 100 || day.windMax >= 100 || day.precipitationChance >= 100);
    
    bool useSmallFont = (tempsBothNegative || tempsBothDouble || dataCrowded || dataHuge);
    
    int dataTextSize = useSmallFont ? 1 : 2;
    int yOffsetData = useSmallFont ? 5 : 0;

    // === WIATR MIN/MAX ===
    tft.setTextSize(dataTextSize); 

    int windX = 200;      
    int currentX = windX; 

    // 1. Min wiatr (DARKGREY)
    tft.setTextColor(TFT_DARKGREY); 
    String minWind = String((int)round(day.windMin));
    tft.drawString(minWind, currentX, y + yOffsetData);
    currentX += tft.textWidth(minWind); 

    // 2. Separator "-" (BIAŁY)
    tft.setTextColor(TFT_WHITE);
    String sep = "-"; 
    tft.drawString(sep, currentX, y + yOffsetData);
    currentX += tft.textWidth(sep);

    // 3. Max wiatr (BIAŁY)
    String maxWind = String((int)round(day.windMax));
    tft.drawString(maxWind, currentX, y + yOffsetData);
    currentX += tft.textWidth(maxWind);

    // 4. Jednostka "km/h"
    tft.setTextSize(1);
    tft.setTextColor(TFT_SILVER);
    int unitCorrection = useSmallFont ? 0 : 5; 
    tft.drawString("km/h", currentX + 2, y + yOffsetData + unitCorrection);
    
    // === OPADY ===
    tft.setTextColor(0x001F);
    tft.setTextDatum(TR_DATUM);
    tft.setTextSize(dataTextSize);
    tft.drawString(String(day.precipitationChance) + "%", 315, y + yOffsetData);
    tft.setTextDatum(TL_DATUM); 
  }
  
  // === FOOTER z lokalizacją ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM); 
  tft.fillRect(0, 200, 320, 25, COLOR_BACKGROUND);
  
  if (locationManager.isLocationSet()) {
    WeatherLocation loc = locationManager.getCurrentLocation();
    String locationText;
    if (loc.displayName.length() > 0 && loc.displayName != loc.cityName) {
        locationText = loc.cityName + ", " + loc.displayName;
    } else {
        locationText = loc.cityName;
    }
    if (locationText.length() > 38) {
      locationText = locationText.substring(0, 35) + "...";
    }
    tft.drawString(locationText, 160, 205);
  } else {
    tft.drawString("Brak lokalizacji", 160, 205);
  }
  Serial.println("✅ Weekly screen rendered");
}

void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
  // Ekran 4: Lokalne czujniki DHT22
  Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS");
  
  tft.fillScreen(COLOR_BACKGROUND);
  
  // === NAGŁÓWEK ===
  tft.setTextColor(TFT_MAGENTA );
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("LOKALNE CZUJNIKI", 160, 10);
  
  // === INFORMACJE O DHT22 (PRAWDZIWE DANE) ===
  DHT22Data dhtData = getDHT22Data();
  
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  
  // 1. Temperatura
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 60);
  tft.print("Temp. lokalna: ");
  tft.setTextColor(TFT_YELLOW);
  if (dhtData.isValid) {
    tft.printf("%.1f'C", dhtData.temperature);
  } else {
    tft.print("--.-'C");
  }
  
  // 2. Wilgotność
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 110);
  tft.print("Wilg. lokalna: ");
  tft.setTextColor(TFT_CYAN);
  if (dhtData.isValid) {
    tft.printf("%.1f%%", dhtData.humidity);
  } else {
    tft.print("--.-%");
  }     
  
  // === FOOTER z informacjami o aktualizacjach ===
  // Tutaj wprowadzamy zmiany z kolorowaniem czasu
  
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  
  tft.fillRect(0, UPDATES_CLEAR_Y, 320, 75, COLOR_BACKGROUND);
  
  // Etykieta AKTUALIZACJE
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("STATUS SYSTEMU:", 160, UPDATES_TITLE_Y);
  
  // 1. Status DHT22 (prawdziwy status)
  String dhtStatus = "DHT22: " + dhtData.status;
  uint16_t statusColor = dhtData.isValid ? TFT_GREEN : TFT_RED;
  tft.setTextColor(statusColor);
  tft.drawString(dhtStatus, 160, UPDATES_DHT22_Y);
  
  // 2. Czujnik odczyt
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Odczyt sensora: co 2s", 160, UPDATES_SENSOR_Y);
  
  // 3. Pogoda bieżąca (CZAS NA BIAŁO)
  unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
  String wPrefix = "Pogoda: ";
  String wTime;
  if (weatherAge < 60) wTime = String(weatherAge) + "s temu";
  else if (weatherAge < 3600) wTime = String(weatherAge / 60) + "min temu";
  else wTime = String(weatherAge / 3600) + "h temu";
  String wSuffix = " (co 10min)";
  
  // Oblicz całkowitą szerokość, aby wyśrodkować
  int wTotalWidth = tft.textWidth(wPrefix + wTime + wSuffix);
  int wX = 160 - (wTotalWidth / 2);
  
  // Rysuj sekwencyjnie
  tft.setTextDatum(TL_DATUM);
  
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString(wPrefix, wX, UPDATES_WEATHER_Y);
  wX += tft.textWidth(wPrefix);
  
  tft.setTextColor(TFT_WHITE); // <--- BIAŁY CZAS
  tft.drawString(wTime, wX, UPDATES_WEATHER_Y);
  wX += tft.textWidth(wTime);
  
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString(wSuffix, wX, UPDATES_WEATHER_Y);
  
  // 4. Prognoza weekly (CZAS NA BIAŁO)
  unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
  String fPrefix = "Pogoda tyg.: ";
  String fTime;
  if (weeklyAge < 60) fTime = String(weeklyAge) + "s temu";
  else if (weeklyAge < 3600) fTime = String(weeklyAge / 60) + "min temu";
  else fTime = String(weeklyAge / 3600) + "h temu";
  String fSuffix = " (co 4h)";
  
  int fTotalWidth = tft.textWidth(fPrefix + fTime + fSuffix);
  int fX = 160 - (fTotalWidth / 2);
  
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString(fPrefix, fX, UPDATES_WEEKLY_Y);
  fX += tft.textWidth(fPrefix);
  
  tft.setTextColor(TFT_WHITE); // <--- BIAŁY CZAS
  tft.drawString(fTime, fX, UPDATES_WEEKLY_Y);
  fX += tft.textWidth(fTime);
  
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString(fSuffix, fX, UPDATES_WEEKLY_Y);
  
  // 5. Stan WiFi
  String wifiStatus;
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    int signalQuality = 0;
    if(rssi <= -100) signalQuality = 0;
    else if(rssi >= -50) signalQuality = 100;
    else signalQuality = 2 * (rssi + 100);
    wifiStatus = "WiFi: " + String(WiFi.SSID()) + " (" + String(signalQuality) + "%)";
  } else {
    wifiStatus = "WiFi: Rozlaczony";
  }
  
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString(wifiStatus, 160, UPDATES_WIFI_Y);

  // ============================================
  // STOPKA: Wersja oprogramowania (Prawy Dolny Róg)
  // ============================================
  
  // Ustawienie punktu odniesienia na prawy dolny róg (Bottom Right)
  tft.setTextDatum(BR_DATUM); 
  tft.setTextSize(1); // Mała, dyskretna czcionka
  
  // Kolor szary (TFT_SILVER lub TFT_DARKGREY), żeby nie raziło w oczy
  tft.setTextColor(TFT_DARKGREY, COLOR_BACKGROUND); 

  // Pobieramy wersję z hardware_config.h i tworzymy napis np. "v1.1"
  String versionText = "v" + String(FIRMWARE_VERSION);

  // Rysujemy: Szerokość ekranu - 5px marginesu, Wysokość - 5px marginesu
  tft.drawString(versionText, tft.width() - 5, tft.height() - 5);
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}