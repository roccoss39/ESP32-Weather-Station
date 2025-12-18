#ifndef LOCATION_CONFIG_H
#define LOCATION_CONFIG_H

#include <Arduino.h>

// Struktura dla predefiniowanych lokalizacji
struct WeatherLocation {
    String cityName;
    String countryCode;
    String displayName;
    float latitude;
    float longitude;
    String timezone;
};

// Dzielnice i obszary Szczecina (4 miejsca po przecinku)
const WeatherLocation SZCZECIN_DISTRICTS[] = {
    {"Szczecin", "PL", "Centrum", 53.4289, 14.5530, "CET-1CEST,M3.5.0/2,M10.5.0/3"},          // Centrum miasta (Plac Grunwaldzki)
    {"Szczecin", "PL", "Zawadzkiego", 53.4590, 14.5430, "CET-1CEST,M3.5.0/2,M10.5.0/3"},    // Os. Zawadzkiego
    {"Szczecin", "PL", "Pogodno", 53.4000, 14.6100, "CET-1CEST,M3.5.0/2,M10.5.0/3"},          // Pogodno
    {"Mierzyn", "PL", "Mierzyn", 53.4920, 14.6130, "CET-1CEST,M3.5.0/2,M10.5.0/3"},         // Mierzyn (gmina)
    {"Plonia", "PL", "Plonia", 53.5060, 14.5890, "CET-1CEST,M3.5.0/2,M10.5.0/3"},           // Plonia (gmina)
    {"Szczecin", "PL", "Gumience", 53.4700, 14.6000, "CET-1CEST,M3.5.0/2,M10.5.0/3"},         // Gumience
    {"Szczecin", "PL", "Dabie", 53.3800, 14.6900, "CET-1CEST,M3.5.0/2,M10.5.0/3"},            // Dabie
    {"Szczecin", "PL", "Niebuszewo", 53.4800, 14.5200, "CET-1CEST,M3.5.0/2,M10.5.0/3"},       // Niebuszewo
    {"Szczecin", "PL", "Prawobreze", 53.4600, 14.6400, "CET-1CEST,M3.5.0/2,M10.5.0/3"},       // Prawobrzeże
    {"Szczecin", "PL", "Zelechowa", 53.3600, 14.6000, "CET-1CEST,M3.5.0/2,M10.5.0/3"},        // Żelechowa
    {"Szczecin", "PL", "Turzyn", 53.4150, 14.5150, "CET-1CEST,M3.5.0/2,M10.5.0/3"},         // Turzyn
    {"Szczecin", "PL", "Klonowica", 53.3700, 14.6200, "CET-1CEST,M3.5.0/2,M10.5.0/3"},        // Klonowica
    {"Szczecin", "PL", "Slowianin", 53.4550, 14.5950, "CET-1CEST,M3.5.0/2,M10.5.0/3"},      // Słowianin
    {"Szczecin", "PL", "Warszewo", 53.4750, 14.5350, "CET-1CEST,M3.5.0/2,M10.5.0/3"},       // Warszewo
    {"Szczecin", "PL", "Arkonia", 53.4250, 14.6250, "CET-1CEST,M3.5.0/2,M10.5.0/3"}         // Arkonia
};

// Dzielnice i obszary Poznania (4 miejsca po przecinku)
const WeatherLocation POZNAN_DISTRICTS[] = {
    {"Poznan", "PL", "Centrum", 52.4064, 16.9252, "CET-1CEST,M3.5.0/2,M10.5.0/3"},        // Centrum miasta
    {"Poznan", "PL", "Stare Miasto", 52.4081, 16.9335, "CET-1CEST,M3.5.0/2,M10.5.0/3"},   // Stare Miasto
    {"Poznan", "PL", "Wilda", 52.3951, 16.9252, "CET-1CEST,M3.5.0/2,M10.5.0/3"},          // Wilda
    {"Poznan", "PL", "Jezyc", 52.4181, 16.9052, "CET-1CEST,M3.5.0/2,M10.5.0/3"},          // Jeżyce
    {"Poznan", "PL", "Grunwald", 52.3764, 16.9252, "CET-1CEST,M3.5.0/2,M10.5.0/3"},       // Grunwald
    {"Poznan", "PL", "Nowe Miasto", 52.4264, 16.9452, "CET-1CEST,M3.5.0/2,M10.5.0/3"},    // Nowe Miasto
    {"Poznan", "PL", "Rataje", 52.4064, 17.0052, "CET-1CEST,M3.5.0/2,M10.5.0/3"},         // Rataje
    {"Poznan", "PL", "Piatkowo", 52.4464, 16.8852, "CET-1CEST,M3.5.0/2,M10.5.0/3"},       // Piątkowo
    {"Poznan", "PL", "Winogrady", 52.4364, 16.8652, "CET-1CEST,M3.5.0/2,M10.5.0/3"},      // Winogrady
    {"Poznan", "PL", "Naramowice", 52.4564, 16.8952, "CET-1CEST,M3.5.0/2,M10.5.0/3"}      // Naramowice
};

const int POZNAN_DISTRICTS_COUNT = sizeof(POZNAN_DISTRICTS) / sizeof(POZNAN_DISTRICTS[0]);

// Obszary Złocieńca i okolic (4 miejsca po przecinku)
const WeatherLocation ZLOCIENIEC_AREAS[] = {
    {"Zlocieniec", "PL", "Centrum", 53.5231, 16.0364, "CET-1CEST,M3.5.0/2,M10.5.0/3"},    // Centrum miasta
    {"Zlocieniec", "PL", "Dworcowa", 53.5241, 16.0334, "CET-1CEST,M3.5.0/2,M10.5.0/3"},   // Okolice dworca
    {"Zlocieniec", "PL", "Koszalinska", 53.5201, 16.0394, "CET-1CEST,M3.5.0/2,M10.5.0/3"}, // Ul. Koszalińska
    {"Zlocieniec", "PL", "Lesna", 53.5281, 16.0424, "CET-1CEST,M3.5.0/2,M10.5.0/3"},      // Dzielnica Leśna
    {"Zlocieniec", "PL", "Polnocna", 53.5311, 16.0344, "CET-1CEST,M3.5.0/2,M10.5.0/3"}    // Północna część
};

const int ZLOCIENIEC_AREAS_COUNT = sizeof(ZLOCIENIEC_AREAS) / sizeof(ZLOCIENIEC_AREAS[0]);

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