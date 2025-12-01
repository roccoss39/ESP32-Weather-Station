#include "weather/weather_data.h"
#include "config/weather_config.h"
#include "config/secrets.h"
#include "config/location_config.h"
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
  
  // Używaj LocationManager dla dynamicznej lokalizacji z coordinates
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  Serial.printf("🌐 Weather API - Using location: %s (%.2f, %.2f)\n", 
                currentLoc.displayName, currentLoc.latitude, currentLoc.longitude);
  
  String urlString = locationManager.buildWeatherURL(WEATHER_API_KEY);
  const char* url = urlString.c_str();
  
  Serial.printf("📡 Weather API URL: %s\n", url);
  
  Serial.println("Getting weather...");
  
  http.begin(url);
  http.setTimeout(5000); // 5 sekund timeout
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // === DEBUG: JSON dump tylko w debug mode ===
    #ifdef DEBUG_WEATHER_API
      Serial.println("=== RAW JSON WEATHER API ===");
      Serial.println(payload);
      Serial.println("=== KONIEC RAW JSON ===");
      Serial.println();
    #endif
    
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
      
      // Dodaj dane wschodu i zachodu słońca
      weather.sunrise = doc["sys"]["sunrise"];
      weather.sunset = doc["sys"]["sunset"];
      
      // --- DANE O OPADACH ---
      weather.rainLastHour = 0;  // Domyślnie brak
      weather.snowLastHour = 0;  // Domyślnie brak
      weather.cloudiness = 0;    // Domyślnie brak
      
      // Sprawdź opady deszczu w ostatniej godzinie
      if (doc["rain"] && doc["rain"]["1h"]) {
        weather.rainLastHour = doc["rain"]["1h"];
        Serial.println("Deszcz (1h): " + String(weather.rainLastHour) + "mm");
      }
      
      // Sprawdź opady śniegu w ostatniej godzinie  
      if (doc["snow"] && doc["snow"]["1h"]) {
        weather.snowLastHour = doc["snow"]["1h"];
        Serial.println("Śnieg (1h): " + String(weather.snowLastHour) + "mm");
      }
      
      // Zachmurzenie
      if (doc["clouds"] && doc["clouds"]["all"]) {
        weather.cloudiness = doc["clouds"]["all"];
        Serial.println("Zachmurzenie: " + String(weather.cloudiness) + "%");
      }
      
      weather.isValid = true;
      weather.lastUpdate = millis();
      
      Serial.println("Weather OK: " + String(weather.temperature) + "°C, odczuwalna: " + String(weather.feelsLike) + "°C, ciśnienie: " + String(weather.pressure) + "hPa");
      Serial.println("Wschód: " + String(weather.sunrise) + ", Zachód: " + String(weather.sunset));
      
      http.end(); // Cleanup
      return true;
    } else {
      Serial.println("Weather JSON parse error");
    }
  } else {
    Serial.println("Weather HTTP error: " + String(httpCode));
  }
  
  http.end(); // ZAWSZE cleanup
  return false;
}