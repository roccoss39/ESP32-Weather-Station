#include "weather/forecast_api.h"
#include "weather/forecast_data.h"
#include "weather/weather_data.h"
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
//unsigned long lastWeeklyUpdate = 0;

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
// === POPRAWIONA FUNKCJA DO PLIKU forecast_api.cpp ===

bool generateWeeklyForecast() {
  Serial.println("🗓️ WYWOŁANIE generateWeeklyForecast() - START");
  
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String urlString = locationManager.buildForecastURL(WEATHER_API_KEY);
  
  http.begin(urlString);
  http.setTimeout(10000);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }
  
  String payload = http.getString();
  JsonDocument doc;
  
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    http.end();
    return false;
  }
  
  JsonArray list = doc["list"];
  
  // Reset output
  weeklyForecast.count = 0;
  weeklyForecast.isValid = false;
  
  const char* dayNames[] = {"Nie", "Pon", "Wto", "Sro", "Czw", "Pia", "Sob"};
  
  struct DayGroup {
    int dayOfWeek = -1;
    float tempMin = 999;
    float tempMax = -999;
    float windMin = 999;
    float windMax = -999;
    bool hasData = false;
    String icons[8];
    int iconCounts[8] = {0};
    int iconIndex = 0;
    int precipSum = 0;
    int itemCount = 0;
  };
  
  DayGroup tempGroups[6]; 
  int tempGroupsCount = 0;
  
  // 1. ZBIERANIE DANYCH (Grupowanie)
  for (int i = 0; i < list.size(); i++) {
    JsonObject item = list[i];
    
    long timestamp = item["dt"];
    struct tm* timeinfo = localtime((time_t*)&timestamp);
    int dayOfWeek = timeinfo->tm_wday;
    
    int currentGroupIdx = -1;
    
    if (tempGroupsCount == 0 || tempGroups[tempGroupsCount - 1].dayOfWeek != dayOfWeek) {
      if (tempGroupsCount < 6) {
        currentGroupIdx = tempGroupsCount;
        tempGroups[currentGroupIdx].dayOfWeek = dayOfWeek;
        tempGroupsCount++;
      }
    } else {
      currentGroupIdx = tempGroupsCount - 1;
    }
    
    if (currentGroupIdx >= 0) {
      DayGroup& group = tempGroups[currentGroupIdx];
      
      float temp = item["main"]["temp"].as<float>();
      float wind = item["wind"]["speed"].as<float>() * 3.6; 
      String icon = item["weather"][0]["icon"].as<String>();
      int precipChance = (int)(item["pop"].as<float>() * 100);
      
      if (!group.hasData) {
        group.tempMin = temp;
        group.tempMax = temp;
        group.windMin = wind;
        group.windMax = wind;
        group.hasData = true;
      } else {
        if (temp < group.tempMin) group.tempMin = temp;
        if (temp > group.tempMax) group.tempMax = temp;
        if (wind < group.windMin) group.windMin = wind;
        if (wind > group.windMax) group.windMax = wind;
      }
      
      // === LOGIKA DLA "DZIŚ" (Index 0) - DOMIESZANIE AKTUALNEJ POGODY ===
      // WAŻNE: Wykonuje się TYLKO jeśli weather.isValid jest true.
      // Przy zmianie miasta ustawimy weather.isValid na false, więc to się NIE wykona dla starego miasta.
      if (currentGroupIdx == 0 && weather.isValid) {
           float currentTemp = weather.temperature;
           
           if (currentTemp < group.tempMin) group.tempMin = currentTemp;
           if (currentTemp > group.tempMax) group.tempMax = currentTemp;
           
           // Opcjonalnie wiatr
           // float currentWind = weather.windSpeed * 3.6;
           // if (currentWind > group.windMax) group.windMax = currentWind;
      }
      // =================================================================

      // Ikony
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
  
  // 2. FILTROWANIE I PRZEPISYWANIE
  for (int i = 0; i < tempGroupsCount; i++) {
    if (weeklyForecast.count >= 5) break; 
    
    // === TWOJA NOWA LOGIKA FILTROWANIA ===
    bool isToday = (i == 0); // Zakładamy, że pierwsza grupa to dzień dzisiejszy
    
    // WARUNEK:
    // Jeśli to DZIŚ -> wystarczy itemCount >= 1
    // Jeśli to PRZYSZŁOŚĆ -> musi być itemCount >= 4
    if ((isToday && tempGroups[i].itemCount >= 1) || (!isToday && tempGroups[i].itemCount >= 4)) {
      
      DayGroup& src = tempGroups[i];
      DailyForecast& dest = weeklyForecast.days[weeklyForecast.count];
      
      dest.dayName = String(dayNames[src.dayOfWeek]);
      dest.tempMin = src.tempMin;
      dest.tempMax = src.tempMax;
      dest.windMin = src.windMin;
      dest.windMax = src.windMax;
      dest.precipitationChance = src.itemCount > 0 ? src.precipSum / src.itemCount : 0;
      
      // Ikona
      dest.icon = "01d";
      int maxCount = 0;
      for (int j = 0; j < src.iconIndex; j++) {
          if (src.iconCounts[j] > maxCount) {
            maxCount = src.iconCounts[j];
            dest.icon = src.icons[j];
          }
      }
      if (dest.precipitationChance >= 40) {
         for(int j=0; j<src.iconIndex; j++) {
             if(src.icons[j].indexOf("10") >= 0 || src.icons[j].indexOf("09") >= 0 || src.icons[j].indexOf("13") >= 0) {
                 dest.icon = src.icons[j];
                 break;
             }
         }
      }
      
      weeklyForecast.count++;
    }
  }

  weeklyForecast.isValid = true;
  weeklyForecast.lastUpdate = millis();
  
  http.end();
  return true;
}