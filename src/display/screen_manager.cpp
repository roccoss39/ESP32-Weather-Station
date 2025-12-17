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
#include "config/location_config.h"

// Singleton instance ScreenManager
static ScreenManager screenManager;

ScreenManager& getScreenManager() {
  return screenManager;
}

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
  
  // Sprawdź wiek danych (informacyjnie)
  unsigned long dataAge = millis() - weeklyForecast.lastUpdate;
  Serial.printf("📊 Weekly data: valid=%s, count=%d, age=%.1fh\n", 
               weeklyForecast.isValid ? "YES" : "NO", weeklyForecast.count, dataAge / 3600000.0);
  
  if (!weeklyForecast.isValid || weeklyForecast.count == 0) {
    // Pokaz error screen dla brakujących danych
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
  
  
  // DYNAMICZNE WYRÓWNANIE - dostosuj wysokość do liczby dni
  int availableHeight = 200; // Wysokość ekranu minus margines
  int startY = 15;
  int rowHeight = weeklyForecast.count > 0 ? (availableHeight / weeklyForecast.count) : 35;
  
  Serial.printf("📐 Wyrównanie ekranu: %d dni, wysokość rzędu: %dpx\n", 
                weeklyForecast.count, rowHeight);
  
  for(int i = 0; i < weeklyForecast.count && i < 5; i++) {
    DailyForecast& day = weeklyForecast.days[i];
    int y = startY + (i * rowHeight);
    
    // === NAZWA DNIA ===
    tft.setTextColor(TFT_WHITE); // Bialy
    tft.setTextSize(2); // Wysokość ~16px
    tft.setTextDatum(TL_DATUM);
    tft.drawString(day.dayName, 10, y);
    
    // === PRAWDZIWE IKONY POGODOWE ===
    extern void drawWeatherIcon(TFT_eSPI& tft, int x, int y, String condition, String iconCode);
    
    int iconX = 55;
    int iconY = y - (rowHeight / 4); // Ikony są specyficzne, zostawiamy ich pozycjonowanie
    
    // Mapowanie ikony na warunek pogodowy
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
    
    // Rysuj ikonę
    drawWeatherIcon(tft, iconX, iconY, condition, day.icon);
    
    // === TEMPERATURY MIN/MAX ===
    tft.setTextSize(2);
    
    // Min temp (szary)
    tft.setTextColor(TFT_DARKGREY); 
    tft.setTextDatum(TL_DATUM);
    tft.drawString(String((int)round(day.tempMin)) + "'", 120, y);
    
    // Separator
    tft.setTextColor(TFT_WHITE);
    tft.drawString("/", 155, y);
    
    // Max temp (bialy)
    tft.setTextColor(TFT_WHITE); 
    tft.drawString(String((int)round(day.tempMax)) + "'", 170, y);
    
    int tMinInt = (int)round(day.tempMin);
    int tMaxInt = (int)round(day.tempMax);

    // Warunek 1: Obie temperatury na minusie (minusy zajmują miejsce)
    bool tempsBothNegative = (tMinInt < 0 && tMaxInt < 0);

    // Warunek 2: Obie temperatury są co najmniej 2-cyfrowe (np. 10, -12, 25)
    // Używamy abs() żeby sprawdzić wielkość liczby bez znaku
    bool tempsBothDouble = (abs(tMinInt) >= 10 && abs(tMaxInt) >= 10);

    // Warunek 3: Tłok w danych wiatru/opadów (wszystko >= 10)
    bool dataCrowded = (day.windMin >= 10 && day.windMax >= 10 && day.precipitationChance >= 10);
    
    // Warunek 4: Ekstremalne wartości (setki)
    bool dataHuge = (day.windMin >= 100 || day.windMax >= 100 || day.precipitationChance >= 100);
    
    // FINALNA DECYZJA: Mała czcionka, jeśli spełniony którykolwiek warunek
    bool useSmallFont = (tempsBothNegative || tempsBothDouble || dataCrowded || dataHuge);
    
    // Konfiguracja
    int dataTextSize = useSmallFont ? 1 : 2;
    int yOffsetData = useSmallFont ? 5 : 0; // Centrowanie w pionie dla małej czcionki

    // === WIATR MIN/MAX (Bez spacji, DarkGrey) ===
    tft.setTextSize(dataTextSize); 

    int windX = 200;      
    int currentX = windX; 

    // 1. Min wiatr (DARKGREY)
    tft.setTextColor(TFT_DARKGREY); // Zmiana na ciemnoszary
    String minWind = String((int)round(day.windMin));
    tft.drawString(minWind, currentX, y + yOffsetData);
    currentX += tft.textWidth(minWind); 

    // 2. Separator "-" (BIAŁY, BEZ SPACJI)
    tft.setTextColor(TFT_WHITE);
    String sep = "-"; // Usunięte spacje
    tft.drawString(sep, currentX, y + yOffsetData);
    currentX += tft.textWidth(sep);

    // 3. Max wiatr (BIAŁY)
    String maxWind = String((int)round(day.windMax));
    tft.drawString(maxWind, currentX, y + yOffsetData);
    currentX += tft.textWidth(maxWind);

    // 4. Jednostka "km/h" (Zawsze mała, szara)
    tft.setTextSize(1);
    tft.setTextColor(TFT_SILVER);
    
    // Jeśli główny tekst mały (offset 5), jednostka równo (+0)
    // Jeśli główny tekst duży (offset 0), jednostka niżej (+5)
    int unitCorrection = useSmallFont ? 0 : 5; 
    
    // Dajemy mały odstęp 2px po liczbie
    tft.drawString("km/h", currentX + 2, y + yOffsetData + unitCorrection);
    
    // === OPADY ===
    tft.setTextColor(0x001F); // Ciemny niebieski
    tft.setTextDatum(TR_DATUM); // Wyrównanie do prawej krawędzi
    
    // Używamy tego samego rozmiaru co wiatr
    tft.setTextSize(dataTextSize);
    
    tft.drawString(String(day.precipitationChance) + "%", 315, y + yOffsetData);
    
    tft.setTextDatum(TL_DATUM); // Reset
  }
  
  // === FOOTER z lokalizacją (WYŚRODKOWANY) ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(2);
  
  // Ustawienie: ŚRODEK-GÓRA (Top Center)
  tft.setTextDatum(TC_DATUM); 
  
  // Wyczyść CAŁY pasek na dole (320px szerokości), aby usunąć stare napisy
  tft.fillRect(0, 200, 320, 25, COLOR_BACKGROUND);
  
  if (locationManager.isLocationSet()) {
    WeatherLocation loc = locationManager.getCurrentLocation();
    String locationText;

    // INTELIGENTNE BUDOWANIE NAZWY
    if (loc.displayName.length() > 0 && loc.displayName != loc.cityName) {
        locationText = loc.cityName + ", " + loc.displayName;
    } else {
        locationText = loc.cityName;
    }
    
    // Skracanie tekstu
    if (locationText.length() > 38) {
      locationText = locationText.substring(0, 35) + "...";
    }
    
    // Rysuj na środku ekranu (X = 160)
    tft.drawString(locationText, 160, 205);
  } else {
    tft.drawString("Brak lokalizacji", 160, 205);
  }
  
  Serial.println("✅ Weekly screen rendered with centered location");
}

void ScreenManager::renderLocalSensorsScreen(TFT_eSPI& tft) {
  // Ekran 4: Lokalne czujniki DHT22
  Serial.println("📱 Rysowanie ekranu: LOCAL SENSORS");
  
  tft.fillScreen(COLOR_BACKGROUND);
  
  // === NAGŁÓWEK ===
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("LOKALNE CZUJNIKI", 160, 10);
  
  // === INFORMACJE O DHT22 ===
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  
  // Temperatura
  tft.setCursor(20, 60);
  tft.print("Temp. lokalna: ");
  tft.setTextColor(TFT_YELLOW);
  tft.print("--.-'C");  
  
  // Wilgotność
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(20, 110);
  tft.print("Wilg. lokalna: ");
  tft.setTextColor(TFT_CYAN);
  tft.print("--%");     
  
  // === FOOTER z informacjami o aktualizacjach ===
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  
  // Wyczyść obszar footera
  tft.fillRect(0, UPDATES_CLEAR_Y, 320, 75, COLOR_BACKGROUND);
  
  // Etykieta AKTUALIZACJE
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("AKTUALIZACJE:", 160, UPDATES_TITLE_Y);
  
  // 1. Status DHT22
  tft.setTextColor(TFT_GREEN);
  tft.drawString("DHT22: Gotowy", 160, UPDATES_DHT22_Y);
  
  // 2. Czujnik odczyt
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Czujnik dht: co 2s", 160, UPDATES_SENSOR_Y);
  
  // 3. Pogoda bieżąca
  extern unsigned long lastWeatherCheckGlobal;
  unsigned long weatherAge = (millis() - lastWeatherCheckGlobal) / 1000;
  String weatherUpdateText;
  if (weatherAge < 60) {
    weatherUpdateText = "Pogoda: " + String(weatherAge) + "s temu (co 10min)";
  } else if (weatherAge < 3600) {
    unsigned long weatherMinutes = weatherAge / 60;
    weatherUpdateText = "Pogoda: " + String(weatherMinutes) + "min temu (co 10min)";
  } else {
    unsigned long weatherHours = weatherAge / 3600;
    weatherUpdateText = "Pogoda: " + String(weatherHours) + "h temu (co 10min)";
  }
  tft.drawString(weatherUpdateText, 160, UPDATES_WEATHER_Y);
  
  // 4. Prognoza weekly
  extern WeeklyForecastData weeklyForecast;
  unsigned long weeklyAge = (millis() - weeklyForecast.lastUpdate) / 1000;
  String weeklyUpdateText;
  if (weeklyAge < 60) {
    weeklyUpdateText = "Pogoda tyg.: " + String(weeklyAge) + "s temu (co 4h)";
  } else if (weeklyAge < 3600) {
    unsigned long weeklyMinutes = weeklyAge / 60;
    weeklyUpdateText = "Pogoda tyg.: " + String(weeklyMinutes) + "min temu (co 4h)";
  } else {
    unsigned long weeklyHours = weeklyAge / 3600;
    weeklyUpdateText = "Pogoda tyg.: " + String(weeklyHours) + "h temu (co 4h)";
  }
  tft.drawString(weeklyUpdateText, 160, UPDATES_WEEKLY_Y);
  
  // 5. Stan WiFi
  String wifiStatus;
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    wifiStatus = "WiFi: " + String(WiFi.SSID()) + " (" + String(rssi) + "dBm)";
  } else {
    wifiStatus = "WiFi: Rozlaczony";
  }
  if (wifiStatus.length() > 30) {
    wifiStatus = wifiStatus.substring(0, 27) + "...";
  }
  tft.drawString(wifiStatus, 160, UPDATES_WIFI_Y);
}

void ScreenManager::renderImageScreen(TFT_eSPI& tft) {
  // Ekran 5: Zdjęcie z GitHub
  displayGitHubImage(tft);
}

void ScreenManager::resetWeatherAndTimeCache() {
  getWeatherCache().resetCache();
  getTimeDisplayCache().resetCache();
  Serial.println("📱 Cache reset: WeatherCache + TimeDisplayCache");
}