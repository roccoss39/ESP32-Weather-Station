#include "config/location_config.h"
#include "config/secrets.h"
#include <Preferences.h>

LocationManager locationManager;

void LocationManager::setLocation(const WeatherLocation& location) {
    Serial.println("🔄 LocationManager::setLocation() called");
    Serial.printf("📍 OLD Location: %s (%.6f, %.6f)\n", 
                  locationSet ? currentLocation.displayName : "NONE", 
                  locationSet ? currentLocation.latitude : 0.0, 
                  locationSet ? currentLocation.longitude : 0.0);
    
    currentLocation = location;
    locationSet = true;
    
    Serial.printf("📍 NEW Location: %s (%.6f, %.6f)\n", 
                  location.displayName, location.latitude, location.longitude);
    
    // Weekly forecast aktualizuje się automatycznie przez error mode system
    
    Serial.println("✅ Location successfully changed!");
}

WeatherLocation LocationManager::getCurrentLocation() const {
    if (!locationSet) {
        return SZCZECIN_DISTRICTS[0]; // Default: Szczecin Centrum
    }
    return currentLocation;
}

void LocationManager::loadLocationFromPreferences() {
    Preferences prefs;
    prefs.begin("weather", false);
    
    if (prefs.isKey("city")) {
        String savedCity = prefs.getString("city", "Warsaw");
        String savedCountry = prefs.getString("country", "PL");
        
        // LOAD CUSTOM COORDINATES if available
        if (prefs.isKey("latitude") && prefs.isKey("longitude")) {
            Serial.println("📍 Loading saved CUSTOM coordinates...");
            
            WeatherLocation customSaved;

            customSaved.cityName = savedCity; 
            customSaved.countryCode = savedCountry; 
            
            customSaved.latitude = prefs.getFloat("latitude", 53.44);
            customSaved.longitude = prefs.getFloat("longitude", 14.56);
            
            // To też jest teraz bezpieczne
            customSaved.displayName = prefs.getString("displayName", "Wlasny GPS");
            customSaved.timezone = "UTC0";
            
            setLocation(customSaved);
            Serial.printf("✅ Custom coordinates loaded: %s (%.6f, %.6f)\n",
                         customSaved.displayName, customSaved.latitude, customSaved.longitude);
            prefs.end();
            return;
        }
        
        Serial.println("Loading saved location: " + savedCity + ", " + savedCountry);
        
        // Find matching location in predefined cities
        bool found = false;
        
        // Check Szczecin districts
        for (int i = 0; i < SZCZECIN_DISTRICTS_COUNT; i++) {
            if (savedCity.equals(SZCZECIN_DISTRICTS[i].cityName) && 
                savedCountry.equals(SZCZECIN_DISTRICTS[i].countryCode)) {
                setLocation(SZCZECIN_DISTRICTS[i]);
                found = true;
                break;
            }
        }
        
        // Note: Only Szczecin districts are available now
        
        if (!found) {
            Serial.println("Saved location not found in predefined cities, checking secrets.h");
            
            // Sprawdź czy lokalizacja z secrets.h jest w naszej liście
            if (findLocationFromSecrets()) {
                Serial.println("Using location from secrets.h: " + String(WEATHER_CITY));
            } else {
                Serial.println("Location from secrets.h not found, using default Warsaw");
                setDefaultLocation();
            }
        }
    } else {
        Serial.println("No saved location, checking secrets.h");
        
        // Pierwszeństwo: sprawdź secrets.h
        if (findLocationFromSecrets()) {
            Serial.println("Using location from secrets.h: " + String(WEATHER_CITY));
        } else {
            Serial.println("Location from secrets.h not found, using default Warsaw");
            setDefaultLocation();
        }
    }
    
    prefs.end();
}

bool LocationManager::findLocationFromSecrets() {
    // Sprawdź czy WEATHER_CITY z secrets.h jest w dzielnicach Szczecina
    String secretsCity = String(WEATHER_CITY);
    String secretsCountry = String(WEATHER_COUNTRY);
    
    // Check Szczecin districts
    for (int i = 0; i < SZCZECIN_DISTRICTS_COUNT; i++) {
        if (secretsCity.equals(SZCZECIN_DISTRICTS[i].cityName) && 
            secretsCountry.equals(SZCZECIN_DISTRICTS[i].countryCode)) {
            setLocation(SZCZECIN_DISTRICTS[i]);
            return true;
        }
    }
    
    return false; // Nie znaleziono w dzielnicach Szczecina
}

void LocationManager::saveLocationToPreferences() {
    if (!locationSet) return;
    
    Preferences prefs;
    prefs.begin("weather", false);
    
    prefs.putString("city", currentLocation.cityName);
    prefs.putString("country", currentLocation.countryCode);
    
    // SAVE COORDINATES for custom locations
    prefs.putFloat("latitude", currentLocation.latitude);
    prefs.putFloat("longitude", currentLocation.longitude);
    prefs.putString("displayName", currentLocation.displayName);
    
    prefs.end();
    Serial.printf("💾 Location saved: %s (%.6f, %.6f)\n", 
                  currentLocation.displayName, currentLocation.latitude, currentLocation.longitude);
}

String LocationManager::buildWeatherURL(const char* apiKey) const {
    WeatherLocation loc = getCurrentLocation();
    
    char url[256];
    // Używaj COORDINATES zamiast city name dla większej precyzji  
    snprintf(url, sizeof(url), 
        "https://api.openweathermap.org/data/2.5/weather?lat=%.2f&lon=%.2f&appid=%s&units=metric&lang=en",
        loc.latitude, loc.longitude, apiKey);
    
    return String(url);
}

String LocationManager::buildForecastURL(const char* apiKey) const {
    WeatherLocation loc = getCurrentLocation();
    
    char url[256];
    // Używaj COORDINATES zamiast city name dla większej precyzji
    snprintf(url, sizeof(url), 
        "https://api.openweathermap.org/data/2.5/forecast?lat=%.2f&lon=%.2f&appid=%s&units=metric&lang=en",
        loc.latitude, loc.longitude, apiKey);
    
    return String(url);
}