#include "weather/forecast_api.h"
#include "weather/forecast_data.h"
#include "config/weather_config.h"
#include "config/secrets.h"
#include "config/location_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Definicje globalnych zmiennych
ForecastData forecast;
WeeklyForecastData weeklyForecast;
unsigned long lastForecastUpdate = 0;
unsigned long lastWeeklyUpdate = 0;

bool getForecast() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for forecast");
    return false;
  }

  HTTPClient http;
  
  // Używaj LocationManager dla dynamicznej lokalizacji z coordinates
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  Serial.printf("🔮 Forecast API - Using location: %s (%.2f, %.2f)\n", 
                currentLoc.displayName, currentLoc.latitude, currentLoc.longitude);
  
  String urlString = locationManager.buildForecastURL(WEATHER_API_KEY);
  const char* url = urlString.c_str();
  
  Serial.printf("📡 Forecast API URL: %s\n", url);
  
  Serial.println("Getting forecast...");
  
  http.begin(url);
  http.setTimeout(10000); // 10 sekund timeout
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // === DEBUG: JSON dump tylko w debug mode ===
    #ifdef DEBUG_WEATHER_API
      Serial.println("=== RAW JSON FORECAST API ===");
      Serial.println(payload);
      Serial.println("=== KONIEC RAW JSON FORECAST ===");
      Serial.println();
    #endif
    
    JsonDocument doc; // Nowa wersja ArduinoJson
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      
      // Wyczyść poprzednie dane
      forecast.count = 0;
      forecast.isValid = false;
      
      JsonArray list = doc["list"];
      
      // Weź pierwsze 5 prognoz
      for (int i = 0; i < 5 && i < list.size(); i++) {
        JsonObject item = list[i];
        
        // Wyciągnij timestamp i przekonwertuj na godzinę
        long timestamp = item["dt"];
        struct tm* timeinfo = localtime((time_t*)&timestamp);
        char timeStr[6];
        strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);
        
        forecast.items[i].time = String(timeStr);
        forecast.items[i].temperature = item["main"]["temp"];
        forecast.items[i].windSpeed = item["wind"]["speed"];
        forecast.items[i].icon = item["weather"][0]["icon"].as<String>();
        
        // Używaj angielskiego description (szczegółowy opis)
        forecast.items[i].description = item["weather"][0]["description"].as<String>();  // "light rain", "clear sky", etc.
        
        // Prawdopodobieństwo opadów (pop = probability of precipitation)
        forecast.items[i].precipitationChance = 0;
        if (item["pop"]) {
          forecast.items[i].precipitationChance = (int)(item["pop"].as<float>() * 100); // Konwersja z 0.0-1.0 na 0-100%
        }
        
        Serial.println("Forecast " + String(i+1) + ": " + forecast.items[i].time + 
                      " - " + String(forecast.items[i].temperature, 1) + "°C - " + 
                      forecast.items[i].icon + " - " + forecast.items[i].description);
        
        forecast.count++;
      }
      
      forecast.isValid = true;
      forecast.lastUpdate = millis();
      
      Serial.println("Forecast OK: " + String(forecast.count) + " items loaded");
      
      http.end(); // Cleanup
      return true;
    } else {
      Serial.println("Forecast JSON parse error");
    }
  } else {
    Serial.println("Forecast HTTP error: " + String(httpCode));
  }
  
  http.end(); // ZAWSZE cleanup
  return false;
}

// === FUNKCJA PRZETWARZANIA 40 PROGNOZ 3H NA 5 DNI ===
bool generateWeeklyForecast() {
  Serial.println("🗓️ WYWOŁANIE generateWeeklyForecast() - START");
  Serial.println("🗓️ Generowanie prognozy 5-dniowej z danych 3h...");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected for weekly forecast");
    return false;
  }

  HTTPClient http;
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  String urlString = locationManager.buildForecastURL(WEATHER_API_KEY);
  
  http.begin(urlString);
  http.setTimeout(10000);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ Weekly forecast HTTP error: " + String(httpCode));
    http.end();
    return false;
  }
  
  String payload = http.getString();
  JsonDocument doc;
  
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println("❌ Weekly forecast JSON parse error");
    http.end();
    return false;
  }
  
  JsonArray list = doc["list"];
  Serial.printf("📊 Otrzymano %d prognoz 3h z API\n", list.size());
  
  // Wyczysc poprzednie dane
  weeklyForecast.count = 0;
  weeklyForecast.isValid = false;
  
  // Nazwy dni tygodnia bez polskich znakow
  const char* dayNames[] = {"Nie", "Pon", "Wto", "Sro", "Czw", "Pia", "Sob"};
  
  // Struktura dla grupowania danych na dzien
  struct DayGroup {
    int dayOfWeek = -1;
    float tempMin = 999;
    float tempMax = -999;
    float windMin = 999;
    float windMax = -999;
    bool hasData = false;  // Flaga czy dzień ma jakiekolwiek dane
    String icons[8];  // max 8 ikon na dzien
    int iconCounts[8] = {0};
    int iconIndex = 0;
    int precipSum = 0;
    int itemCount = 0;
  };
  
  DayGroup dayGroups[5];
  int currentDayIndex = -1;
  
  // Przetwarz do 40 prognoz (5 dni × 8 prognoz)
  for (int i = 0; i < min(40, (int)list.size()) && weeklyForecast.count < 5; i++) {
    JsonObject item = list[i];
    
    long timestamp = item["dt"];
    struct tm* timeinfo = localtime((time_t*)&timestamp);
    int dayOfWeek = timeinfo->tm_wday;
    
    // Sprawdz czy to nowy dzien
    if (currentDayIndex == -1 || dayGroups[currentDayIndex].dayOfWeek != dayOfWeek) {
      if (weeklyForecast.count < 5) {
        currentDayIndex = weeklyForecast.count;
        dayGroups[currentDayIndex].dayOfWeek = dayOfWeek;
        weeklyForecast.count++;
        Serial.printf("📅 Nowy dzien %d: %s\n", weeklyForecast.count, dayNames[dayOfWeek]);
      }
    }
    
    if (currentDayIndex >= 0 && currentDayIndex < 5) {
      DayGroup& group = dayGroups[currentDayIndex];
      
      // Zbierz dane pogodowe
      float temp = item["main"]["temp"];
      float wind = item["wind"]["speed"];
      wind = wind * 3.6; // m/s na km/h
      String icon = item["weather"][0]["icon"].as<String>();
      int precipChance = 0;
      if (item["pop"]) {
        precipChance = (int)(item["pop"].as<float>() * 100);
      }
      
      // Aktualizuj min/max temp - inicjalizuj pierwszą wartością jeśli to pierwsze dane
      if (!group.hasData) {
        group.tempMin = temp;  // Inicjalizuj pierwszą prawdziwą temperaturą
        group.tempMax = temp;
        group.windMin = wind;
        group.windMax = wind;
        group.hasData = true;
        Serial.printf("🌡️ Inicjalizacja dnia %s: temp=%.1f°C, wiatr=%.0fkm/h\n", 
                     dayNames[dayOfWeek], temp, wind);
      } else {
        Serial.printf("📊 Aktualizacja dnia %s: temp=%.1f°C (min=%.1f, max=%.1f), wiatr=%.0fkm/h (min=%.0f, max=%.0f)\n", 
                     dayNames[dayOfWeek], temp, group.tempMin, group.tempMax, wind, group.windMin, group.windMax);
        
        // Normalne porównywanie min/max dla kolejnych prognoz
        if (temp < group.tempMin) {
          group.tempMin = temp;
          Serial.printf("❄️ Nowa temp MIN dla %s: %.1f°C\n", dayNames[dayOfWeek], temp);
        }
        if (temp > group.tempMax) {
          group.tempMax = temp;
          Serial.printf("🔥 Nowa temp MAX dla %s: %.1f°C\n", dayNames[dayOfWeek], temp);
        }
        if (wind < group.windMin) {
          group.windMin = wind;
          Serial.printf("🍃 Nowy wiatr MIN dla %s: %.0fkm/h\n", dayNames[dayOfWeek], wind);
        }
        if (wind > group.windMax) {
          group.windMax = wind;
          Serial.printf("💨 Nowy wiatr MAX dla %s: %.0fkm/h\n", dayNames[dayOfWeek], wind);
        }
      }
      
      // Zlicz ikony (znajdz dominujaca)
      bool iconFound = false;
      for (int j = 0; j < group.iconIndex; j++) {
        if (group.icons[j] == icon) {
          group.iconCounts[j]++;
          iconFound = true;
          break;
        }
      }
      if (!iconFound && group.iconIndex < 8) {
        group.icons[group.iconIndex] = icon;
        group.iconCounts[group.iconIndex] = 1;
        group.iconIndex++;
      }
      
      group.precipSum += precipChance;
      group.itemCount++;
    }
  }
  
  // USUŃ OSTATNI DZIEŃ JEŚLI MA ZA MAŁO PROGNOZ (< 3)
  if (weeklyForecast.count > 0) {
    DayGroup& lastDay = dayGroups[weeklyForecast.count - 1];
    if (lastDay.itemCount < 3) {
      Serial.printf("⚠️ Usuwam ostatni dzień %s - za mało prognoz (%d)\n", 
                    dayNames[lastDay.dayOfWeek], lastDay.itemCount);
      weeklyForecast.count--; // Usuń ostatni dzień
    }
  }
  
  // Przekonwertuj zgrupowane dane na finalna strukture
  for (int i = 0; i < weeklyForecast.count; i++) {
    DayGroup& group = dayGroups[i];
    DailyForecast& day = weeklyForecast.days[i];
    
    day.dayName = String(dayNames[group.dayOfWeek]);
    
    // Sprawdź czy dzień ma prawidłowe dane
    if (!group.hasData || group.tempMin == 999 || group.tempMax == -999) {
      Serial.printf("⚠️ BŁĄD: Dzień %s ma nieprawidłowe dane temp: min=%.1f, max=%.1f\n", 
                   day.dayName.c_str(), group.tempMin, group.tempMax);
      // Ustaw wartości domyślne/sensowne
      day.tempMin = 0;
      day.tempMax = 0;
    } else {
      day.tempMin = group.tempMin;
      day.tempMax = group.tempMax;
    }
    day.windMin = group.windMin;
    day.windMax = group.windMax;
    day.precipitationChance = group.itemCount > 0 ? group.precipSum / group.itemCount : 0;
    
    // ULEPSZONA LOGIKA: Priorytetyzuj ikony opadów przy wysokim prawdopodobieństwie
    day.icon = "01d"; // Domyślnie słońce
    bool precipIconFound = false;
    
    if (day.precipitationChance >= 40) {
      // Przy wysokim prawdopodobieństwie opadów (≥40%) szukaj ikon opadów w kolejności priorytetów
      String precipIcons[] = {"11", "10", "09", "13"}; // burza > deszcz > mżawka > śnieg
      
      for (int p = 0; p < 4 && !precipIconFound; p++) {
        for (int j = 0; j < group.iconIndex; j++) {
          if (group.icons[j].indexOf(precipIcons[p]) >= 0) {
            day.icon = group.icons[j];
            precipIconFound = true;
            Serial.printf("🌧️ Wybrano ikonę opadów %s (prawdopodobieństwo: %d%%)\n", 
                         day.icon.c_str(), day.precipitationChance);
            break; // Znaleziono odpowiednią ikonę opadów
          }
        }
      }
    }
    
    // Jeśli brak ikon opadów lub niskie prawdopodobieństwo, użyj dominującej ikony
    if (!precipIconFound) {
      int maxCount = 0;
      for (int j = 0; j < group.iconIndex; j++) {
        if (group.iconCounts[j] > maxCount) {
          maxCount = group.iconCounts[j];
          day.icon = group.icons[j];
        }
      }
    }
    
    Serial.printf("✅ Dzien %d: %s, %.1f'-%.1f'C, %.0f-%.0fkm/h, %s, %d%% (%d prognoz)\n", 
                  i+1, day.dayName.c_str(), day.tempMin, day.tempMax, 
                  day.windMin, day.windMax, day.icon.c_str(), day.precipitationChance, group.itemCount);
  }
  
  weeklyForecast.isValid = true;
  weeklyForecast.lastUpdate = millis();
  
  Serial.println("✅ Prognoza 5-dniowa wygenerowana: " + String(weeklyForecast.count) + " dni");
  
  http.end();
  return true;
}