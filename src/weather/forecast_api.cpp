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
unsigned long lastForecastUpdate = 0;

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