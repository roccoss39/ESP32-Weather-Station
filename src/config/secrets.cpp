#include "config/secrets.h"

// --- DEFINICJE ZMIENNYCH Z SECRETS ---
// Te wartości są teraz w .cpp file żeby uniknąć multiple definition

// --- KONFIGURACJA WIFI ---
const char* WIFI_SSID = "zero";
const char* WIFI_PASSWORD = "Qweqweqwe1";

// --- KLUCZ API OPENWEATHERMAP ---
const char* WEATHER_API_KEY = "ac44d6e8539af12c769627cbdfbbbe56";

// --- LOKALIZACJA ---
const char* WEATHER_CITY = "Szczecin";
const char* WEATHER_COUNTRY = "PL";
const char* WEATHER_LANGUAGE = "en";

// --- NTP SERWER ---
const char* NTP_SERVER = "pool.ntp.org";
const char* TIMEZONE_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3";