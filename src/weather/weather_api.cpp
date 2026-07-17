#include "weather/weather_api.h"
#include "weather/weather_data.h"
#include "config/location_config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WeatherData weather;

// Lokalny tłumacz ikon dla bieżącej pogody
static String getWmoIconCurrent(int code, int is_day) {
    String suffix = is_day ? "d" : "n";
    if (code == 0) return "01" + suffix; 
    if (code == 1) return "02" + suffix; 
    if (code == 2) return "03" + suffix; 
    if (code == 3) return "04" + suffix; 
    if (code == 45 || code == 48) return "50" + suffix; 
    if (code >= 51 && code <= 57) return "09" + suffix; 
    if (code >= 61 && code <= 65) return "10" + suffix; 
    if (code >= 66 && code <= 77) return "13" + suffix; 
    if (code >= 80 && code <= 82) return "09" + suffix; 
    if (code >= 85 && code <= 86) return "13" + suffix; 
    if (code >= 95 && code <= 99) return "11" + suffix; 
    return "01" + suffix;
}

static String getWmoDescCurrent(int code) {
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

bool getWeather() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ Brak WiFi - nie pobieram glownej pogody");
        return false;
    }

    HTTPClient http;
    WeatherLocation currentLoc = locationManager.getCurrentLocation();
    
    // Zapytanie do Open-Meteo z automatyczną strefą czasową (timezone=auto)
    // Pobieramy parametry wiatru, temperaturę, wilgotność oraz wschody/zachody słońca w formacie UNIX
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) + 
                 "&longitude=" + String(currentLoc.longitude, 4) + 
                 "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,wind_speed_10m,wind_direction_10m&daily=sunrise,sunset&timezone=auto&wind_speed_unit=ms&timeformat=unixtime";

    Serial.println("🌐 Main Weather API (Open-Meteo) URL: " + url);
    
    http.begin(url);
    http.setTimeout(8000);
    int httpCode = http.GET();
    yield();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        yield();
        
        // Super-oszczędny filtr RAM chroniący stację przed wyczerpaniem pamięci
        JsonDocument filter;
        filter["current"]["temperature_2m"] = true;
        filter["current"]["relative_humidity_2m"] = true;
        filter["current"]["apparent_temperature"] = true;
        filter["current"]["is_day"] = true;
        filter["current"]["weather_code"] = true;
        filter["current"]["wind_speed_10m"] = true;
        filter["current"]["wind_direction_10m"] = true;
        filter["daily"]["sunrise"] = true;
        filter["daily"]["sunset"] = true;
        filter["utc_offset_seconds"] = true; // Potrzebne do wyliczenia poprawnej strefy czasowej
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        yield();
        
        if (!error) {
            weather.temperature = doc["current"]["temperature_2m"].as<float>();
            weather.feelsLike = doc["current"]["apparent_temperature"].as<float>();
            weather.humidity = doc["current"]["relative_humidity_2m"].as<int>();
            weather.windSpeed = doc["current"]["wind_speed_10m"].as<float>();
            weather.windDeg = doc["current"]["wind_direction_10m"].as<int>();
            
            int code = doc["current"]["weather_code"].as<int>();
            int is_day = doc["current"]["is_day"].as<int>();
            
            weather.icon = getWmoIconCurrent(code, is_day);
            weather.description = getWmoDescCurrent(code);
            
            // Wschód i zachód słońca
            weather.sunrise = doc["daily"]["sunrise"][0].as<long>();
            weather.sunset = doc["daily"]["sunset"][0].as<long>();
            
            // Zapisujemy przesunięcie strefy czasowej (np. +7200 sekund dla UTC+2 w lecie)
            weather.timezone = doc["utc_offset_seconds"].as<long>();
            
            weather.isValid = true;
            Serial.println("✅ Main Weather OK: Dane pobrane z Open-Meteo!");
            http.end();
            return true;
        } else {
            Serial.println("❌ JSON parse error: " + String(error.c_str()));
        }
    } else {
        Serial.println("❌ HTTP error: " + String(httpCode));
    }
    
    http.end();
    return false;
}