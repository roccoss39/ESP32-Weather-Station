#ifndef LOCATION_CONFIG_H
#define LOCATION_CONFIG_H

#include <Arduino.h>

// Struktura dla predefiniowanych lokalizacji
struct WeatherLocation {
    const char* cityName;
    const char* countryCode;
    const char* displayName;
    float latitude;
    float longitude;
    const char* timezone;
};

// Dzielnice i obszary Szczecina
const WeatherLocation SZCZECIN_DISTRICTS[] = {
    {"Szczecin", "PL", "Centrum", 53.44, 14.56, "CET-1CEST,M3.5.0/2,M10.5.0/3"},        // Centrum miasta
    {"Szczecin", "PL", "Stare Miasto", 53.43, 14.55, "CET-1CEST,M3.5.0/2,M10.5.0/3"},   // Stare Miasto
    {"Szczecin", "PL", "Śródmieście", 53.42, 14.57, "CET-1CEST,M3.5.0/2,M10.5.0/3"},    // Śródmieście
    {"Szczecin", "PL", "Pogodno", 53.40, 14.61, "CET-1CEST,M3.5.0/2,M10.5.0/3"},        // Pogodno
    {"Szczecin", "PL", "Gumieńce", 53.47, 14.60, "CET-1CEST,M3.5.0/2,M10.5.0/3"},       // Gumieńce
    {"Szczecin", "PL", "Dąbie", 53.38, 14.69, "CET-1CEST,M3.5.0/2,M10.5.0/3"},          // Dąbie
    {"Szczecin", "PL", "Niebuszewo", 53.48, 14.52, "CET-1CEST,M3.5.0/2,M10.5.0/3"},     // Niebuszewo
    {"Szczecin", "PL", "Prawobrzeże", 53.46, 14.64, "CET-1CEST,M3.5.0/2,M10.5.0/3"},    // Prawobrzeże
    {"Szczecin", "PL", "Żelechowa", 53.36, 14.60, "CET-1CEST,M3.5.0/2,M10.5.0/3"},      // Żelechowa
    {"Szczecin", "PL", "Port/Nabrzeże", 53.41, 14.58, "CET-1CEST,M3.5.0/2,M10.5.0/3"}  // Port i nabrzeże
};

// Placeholder dla zachowania kompatybilności
const WeatherLocation EUROPE_CITIES[] = {};
const WeatherLocation USA_CITIES[] = {};

#define SZCZECIN_DISTRICTS_COUNT (sizeof(SZCZECIN_DISTRICTS) / sizeof(SZCZECIN_DISTRICTS[0]))
#define EUROPE_CITIES_COUNT 0
#define USA_CITIES_COUNT 0

// Current location management
class LocationManager {
private:
    WeatherLocation currentLocation;
    bool locationSet = false;

public:
    void setLocation(const WeatherLocation& location);
    WeatherLocation getCurrentLocation() const;
    bool isLocationSet() const { return locationSet; }
    
    // Load/save from Preferences
    void loadLocationFromPreferences();
    void saveLocationToPreferences();
    
    // Default location
    void setDefaultLocation() {
        setLocation(SZCZECIN_DISTRICTS[0]); // Default: Szczecin Centrum
    }
    
    // Find location from secrets.h
    bool findLocationFromSecrets();
    
    // Build weather API URL
    String buildWeatherURL(const char* apiKey) const;
    String buildForecastURL(const char* apiKey) const;
};

// Global instance
extern LocationManager locationManager;

#endif