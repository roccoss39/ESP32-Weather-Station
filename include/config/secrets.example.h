#ifndef SECRETS_H
#define SECRETS_H

// 📋 TEMPLATE: Skopiuj ten plik jako secrets.h i uzupełnij prawdziwe dane

// --- KONFIGURACJA WIFI ---
const char* eWIFI_SSID = "your_network_name";           // Nazwa Twojej sieci WiFi
const char* WIFI_PASSWORD = "your_wifi_password";      // Hasło do WiFi

// --- KLUCZ API OPENWEATHERMAP ---
// Zarejestruj się na: https://openweathermap.org/api
const char* WEATHER_API_KEY = "your_openweathermap_api_key";

// --- LOKALIZACJA ---
const char* WEATHER_CITY = "Szczecin";       // Twoje miasto
const char* WEATHER_COUNTRY = "PL";        // Kod kraju (PL, US, DE, etc.)
const char* WEATHER_LANGUAGE = "en";       // Język (en, pl, de, etc.)

// --- NTP SERWER ---
const char* NTP_SERVER = "pool.ntp.org";
const char* TIMEZONE_INFO = "CET-1CEST,M3.5.0/2,M10.5.0/3";  // Europa/Warsaw

#endif