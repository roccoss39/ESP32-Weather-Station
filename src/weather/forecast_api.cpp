#include "weather/forecast_api.h"
#include "weather/forecast_data.h"
#include "weather/weather_data.h"
#include "config/location_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config/debug_config.h"

// --- PRZEŁĄCZNIK TRYBU TESTOWEGO ---
#define ENABLE_FORECAST_MOCK 0 // Zmień na 0, aby wrócić do prawdziwych danych z internetu

// Definicje globalnych zmiennych
ForecastData forecast;
WeeklyForecastData weeklyForecast;
unsigned long lastForecastUpdate = 0;

// === LOKALNY TŁUMACZ KODÓW WMO (Open-Meteo) NA IKONY ===
static String getWmoIconLocal(int code, int is_day) {
    String suffix = is_day ? "d" : "n";
    if (code == 0) return "01" + suffix; // Bezchmurnie
    if (code == 1) return "02" + suffix; // Głównie bezchmurnie
    if (code == 2) return "03" + suffix; // Częściowe zachmurzenie
    if (code == 3) return "04" + suffix; // Pochmurno
    if (code == 45 || code == 48) return "50" + suffix; // Mgła
    if (code >= 51 && code <= 57) return "09" + suffix; // Mżawka
    if (code >= 61 && code <= 65) return "10" + suffix; // Deszcz
    if (code >= 66 && code <= 77) return "13" + suffix; // Śnieg
    if (code >= 80 && code <= 82) return "09" + suffix; // Ulewa
    if (code >= 85 && code <= 86) return "13" + suffix; // Śnieżyca
    if (code >= 95 && code <= 99) return "11" + suffix; // Burza
    return "01" + suffix;
}

static String getWmoDescLocal(int code) {
    if (code == 0) return "clear sky";
    if (code == 1) return "few clouds";
    if (code == 2) return "scattered clouds";
    if (code == 3) return "overcast clouds";
    if (code == 45 || code == 48) return "fog";
    if (code >= 51 && code <= 57) return "drizzle";
    if (code >= 61 && code <= 65) return "rain";
    if (code >= 66 && code <= 77) return "snow";
    if (code >= 80 && code <= 82) return "shower rain";
    if (code >= 85 && code <= 86) return "snow showers";
    if (code >= 95 && code <= 99) return "thunderstorm";
    return "unknown";
}

// =========================================================================
// 1. PROGNOZA CO 3 GODZINY (NA WYKRES)
// =========================================================================
bool getForecast() {
  #if ENABLE_FORECAST_MOCK == 1
    Serial.println("🧪 [MOCK] Generowanie testowej prognozy 3-godzinnej (Wykres)...");
    
    forecast.count = 5;
    
    // Progresja chmur na wykresie! Od pełnego słońca do burzy
    // Test 1: Słońce (01d)
    forecast.items[0].time = "12:00"; forecast.items[0].temperature = 25.0; forecast.items[0].windSpeed = 5.0; forecast.items[0].icon = "01d"; forecast.items[0].description = "clear sky"; forecast.items[0].precipitationChance = 0;
    // Test 2: Małe chmurki (02d)
    forecast.items[1].time = "15:00"; forecast.items[1].temperature = 23.5; forecast.items[1].windSpeed = 12.0; forecast.items[1].icon = "02d"; forecast.items[1].description = "few clouds"; forecast.items[1].precipitationChance = 5;
    // Test 3: Średnie zachmurzenie (03d)
    forecast.items[2].time = "18:00"; forecast.items[2].temperature = 20.0; forecast.items[2].windSpeed = 22.0; forecast.items[2].icon = "03d"; forecast.items[2].description = "scattered clouds"; forecast.items[2].precipitationChance = 15;
    // Test 4: Pochmurno (04n - noc)
    forecast.items[3].time = "21:00"; forecast.items[3].temperature = 16.5; forecast.items[3].windSpeed = 35.0; forecast.items[3].icon = "04n"; forecast.items[3].description = "overcast clouds"; forecast.items[3].precipitationChance = 45;
    // Test 5: Burza (11n - noc)
    forecast.items[4].time = "00:00"; forecast.items[4].temperature = 12.0; forecast.items[4].windSpeed = 65.0; forecast.items[4].icon = "11n"; forecast.items[4].description = "thunderstorm"; forecast.items[4].precipitationChance = 95;

    forecast.isValid = true;
    forecast.lastUpdate = millis();
    return true;
  #endif

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected for forecast");
    return false;
  }

  HTTPClient http;
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  
  // Wiatr pobieramy w m/s, bo tak oczekuje tego interfejs wykresu (który potem robi * 3.6)
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) +
               "&longitude=" + String(currentLoc.longitude, 4) +
               "&hourly=temperature_2m,weather_code,wind_speed_10m,precipitation_probability,is_day&forecast_hours=16&timezone=auto&wind_speed_unit=ms";
  
  Serial.println("🔮 Hourly Forecast API (Open-Meteo) URL: " + url);
  
  http.begin(url);
  http.setTimeout(8000); 
  int httpCode = http.GET();
  yield(); 
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    yield();
    
    JsonDocument filter;
    filter["hourly"]["time"] = true;
    filter["hourly"]["temperature_2m"] = true;
    filter["hourly"]["weather_code"] = true;
    filter["hourly"]["wind_speed_10m"] = true;
    filter["hourly"]["precipitation_probability"] = true;
    filter["hourly"]["is_day"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    yield();
    
    #ifdef DEBUG_WEATHER_API
    Serial.println(payload);
    #endif

    if (!error) {
      forecast.count = 0;
      forecast.isValid = false;
      
      JsonArray timeArr = doc["hourly"]["time"];
      JsonArray tempArr = doc["hourly"]["temperature_2m"];
      JsonArray codeArr = doc["hourly"]["weather_code"];
      JsonArray windArr = doc["hourly"]["wind_speed_10m"];
      JsonArray precipArr = doc["hourly"]["precipitation_probability"];
      JsonArray isDayArr = doc["hourly"]["is_day"];
      
      for (int i = 0; i < 5; i++) {
        int idx = (i + 1) * 3; 
        if (idx >= timeArr.size()) break;
        
        String tStr = timeArr[idx].as<String>();
        forecast.items[i].time = tStr.substring(11, 16); 
        forecast.items[i].temperature = tempArr[idx].as<float>();
        forecast.items[i].windSpeed = windArr[idx].as<float>();
        
        int code = codeArr[idx].as<int>();
        int is_day = isDayArr[idx].as<int>();
        
        forecast.items[i].icon = getWmoIconLocal(code, is_day);
        forecast.items[i].description = getWmoDescLocal(code);
        forecast.items[i].precipitationChance = precipArr[idx].as<int>();
        
        forecast.count++;
      }
      
      forecast.isValid = true;
      forecast.lastUpdate = millis();
      
      Serial.println("✅ Hourly Forecast OK: 5 items loaded");
      http.end();
      return true;
    } else {
      Serial.println("❌ Forecast JSON parse error: " + String(error.c_str()));
    }
  } else {
    Serial.println("❌ Forecast HTTP error: " + String(httpCode));
  }
  
  http.end(); 
  return false;
}

// =========================================================================
// 2. PROGNOZA NA NAJBLIŻSZE DNI (DLA EKRANU TYGODNIOWEGO)
// =========================================================================
bool generateWeeklyForecast() {
  #if ENABLE_FORECAST_MOCK == 1
    Serial.println("🧪 [MOCK] Generowanie testowej prognozy 5-dniowej...");
    
    weeklyForecast.count = 5;
    
    // Pogodowy rollercoaster: Słońce -> Chmury -> Deszcz -> Burza -> Śnieg
    weeklyForecast.days[0].dayName = "Pon"; weeklyForecast.days[0].tempMin = 15.0; weeklyForecast.days[0].tempMax = 28.0; weeklyForecast.days[0].windMin = 5.0; weeklyForecast.days[0].windMax = 15.0; weeklyForecast.days[0].icon = "01d"; weeklyForecast.days[0].precipitationChance = 0;
    weeklyForecast.days[1].dayName = "Wto"; weeklyForecast.days[1].tempMin = 12.0; weeklyForecast.days[1].tempMax = 22.0; weeklyForecast.days[1].windMin = 15.0; weeklyForecast.days[1].windMax = 35.0; weeklyForecast.days[1].icon = "03d"; weeklyForecast.days[1].precipitationChance = 20;
    weeklyForecast.days[2].dayName = "Sro"; weeklyForecast.days[2].tempMin = 8.0; weeklyForecast.days[2].tempMax = 14.0; weeklyForecast.days[2].windMin = 25.0; weeklyForecast.days[2].windMax = 55.0; weeklyForecast.days[2].icon = "10d"; weeklyForecast.days[2].precipitationChance = 85;
    weeklyForecast.days[3].dayName = "Czw"; weeklyForecast.days[3].tempMin = 2.0; weeklyForecast.days[3].tempMax = 9.0; weeklyForecast.days[3].windMin = 40.0; weeklyForecast.days[3].windMax = 95.0; weeklyForecast.days[3].icon = "11d"; weeklyForecast.days[3].precipitationChance = 100;
    weeklyForecast.days[4].dayName = "Pia"; weeklyForecast.days[4].tempMin = -5.0; weeklyForecast.days[4].tempMax = 1.0; weeklyForecast.days[4].windMin = 15.0; weeklyForecast.days[4].windMax = 45.0; weeklyForecast.days[4].icon = "13d"; weeklyForecast.days[4].precipitationChance = 90;

    weeklyForecast.isValid = true;
    weeklyForecast.lastUpdate = millis();
    return true;
  #endif

  Serial.println("🗓️ WYWOŁANIE generateWeeklyForecast() - START (Open-Meteo)");
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  WeatherLocation currentLoc = locationManager.getCurrentLocation();
  
  // Pobieramy dane OD RAZU w km/h, bo tego oczekuje ekran tygodniowy
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) +
               "&longitude=" + String(currentLoc.longitude, 4) +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,wind_speed_10m_max,precipitation_probability_max&timezone=auto&forecast_days=5&wind_speed_unit=kmh";
  
  Serial.println("📡 Weekly API URL: " + url);
  http.begin(url);
  http.setTimeout(8000);
  int httpCode = http.GET();
  yield();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    yield();
    
    JsonDocument filter;
    filter["daily"]["time"] = true;
    filter["daily"]["weather_code"] = true;
    filter["daily"]["temperature_2m_max"] = true;
    filter["daily"]["temperature_2m_min"] = true;
    filter["daily"]["wind_speed_10m_max"] = true;
    filter["daily"]["precipitation_probability_max"] = true;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    yield();
    
    #ifdef DEBUG_WEATHER_API
    Serial.println(payload);
    #endif

    if (!error) {
      weeklyForecast.count = 0;
      weeklyForecast.isValid = false;
      
      JsonArray timeArr = doc["daily"]["time"];
      JsonArray codeArr = doc["daily"]["weather_code"];
      JsonArray maxTArr = doc["daily"]["temperature_2m_max"];
      JsonArray minTArr = doc["daily"]["temperature_2m_min"];
      JsonArray maxWArr = doc["daily"]["wind_speed_10m_max"];
      JsonArray popArr = doc["daily"]["precipitation_probability_max"];
      
      const char* dayNames[] = {"Nie", "Pon", "Wto", "Sro", "Czw", "Pia", "Sob"};
      
      for (int i = 0; i < 5; i++) {
        if (i >= timeArr.size()) break;
        
        String dateStr = timeArr[i].as<String>();
        
        struct tm tm_date = {0};
        tm_date.tm_year = dateStr.substring(0, 4).toInt() - 1900;
        tm_date.tm_mon = dateStr.substring(5, 7).toInt() - 1;
        tm_date.tm_mday = dateStr.substring(8, 10).toInt();
        mktime(&tm_date);
        int wday = tm_date.tm_wday; 
        
        weeklyForecast.days[i].dayName = String(dayNames[wday]);
        weeklyForecast.days[i].tempMax = maxTArr[i].as<float>();
        weeklyForecast.days[i].tempMin = minTArr[i].as<float>();
        
        float windMax = maxWArr[i].as<float>();
        weeklyForecast.days[i].windMax = windMax;
        weeklyForecast.days[i].windMin = windMax * 0.4; 
        
        int code = codeArr[i].as<int>();
        weeklyForecast.days[i].icon = getWmoIconLocal(code, 1); 
        weeklyForecast.days[i].precipitationChance = popArr[i].as<int>();
        
        // === LOGIKA DLA "DZIŚ" (Index 0) - DOMIESZANIE AKTUALNEJ POGODY ===
        if (i == 0 && weather.isValid) {
           float currentTemp = weather.temperature;
           if (currentTemp < weeklyForecast.days[i].tempMin) weeklyForecast.days[i].tempMin = currentTemp;
           if (currentTemp > weeklyForecast.days[i].tempMax) weeklyForecast.days[i].tempMax = currentTemp;
        }
        
        weeklyForecast.count++;
      }
      
      weeklyForecast.isValid = true;
      weeklyForecast.lastUpdate = millis();
      Serial.println("✅ Weekly Forecast OK: Załadowano 5 dni z Open-Meteo!");
      http.end();
      return true;
    } else {
        Serial.println("❌ Weekly JSON parse error: " + String(error.c_str()));
    }
  } else {
      Serial.println("❌ Weekly HTTP error: " + String(httpCode));
  }
  
  http.end();
  return false;
}