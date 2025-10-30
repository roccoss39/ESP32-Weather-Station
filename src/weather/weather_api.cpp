#include "weather/weather_data.h"
#include "config/weather_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Definicje globalnych zmiennych
WeatherData weather;
unsigned long lastWeatherUpdate = 0;

bool getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + 
               String(WEATHER_CITY) + "," + String(WEATHER_COUNTRY) + 
               "&appid=" + String(WEATHER_API_KEY) + 
               "&units=metric&lang=" + String(WEATHER_LANGUAGE);
  
  Serial.println("Getting weather...");
  
  http.begin(url);
  http.setTimeout(5000); // 5 sekund timeout
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // === DEBUG: WYŚWIETL CAŁY JSON Z API ===
    Serial.println("=== RAW JSON WEATHER API ===");
    Serial.println(payload);
    Serial.println("=== KONIEC RAW JSON ===");
    Serial.println();
    
    JsonDocument doc;
    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      weather.temperature = doc["main"]["temp"];
      weather.feelsLike = doc["main"]["feels_like"];  // Temperatura odczuwalna
      weather.humidity = doc["main"]["humidity"];
      weather.pressure = doc["main"]["pressure"];  // Ciśnienie w hPa
      
      // Używaj angielskiego description (szczegółowy opis)
      weather.description = doc["weather"][0]["description"].as<String>();  // "light rain", "clear sky", etc.
      weather.windSpeed = doc["wind"]["speed"];
      
      // Dodaj kod ikony z API
      if (doc["weather"][0]["icon"]) {
        weather.icon = doc["weather"][0]["icon"].as<String>();
        Serial.println("Ikona API: '" + weather.icon + "' dla kategorii: '" + weather.description + "'");
      }
      
      weather.isValid = true;
      weather.lastUpdate = millis();
      
      Serial.println("Weather OK: " + String(weather.temperature) + "°C, odczuwalna: " + String(weather.feelsLike) + "°C, ciśnienie: " + String(weather.pressure) + "hPa");
      http.end();
      return true;
    }
  }
  
  Serial.println("Weather failed: " + String(httpCode));
  http.end();
  return false;
}