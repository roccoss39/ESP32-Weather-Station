#include "weather/open_meteo_api.h"
#include "config/location_config.h" 
#include "weather/weather_data.h"
#include "display/display_pressure.h" // Niezbędne do odczytu showPressureAtSeaLevel
#include <HTTPClient.h>
#include <WiFiClient.h> 
#include <ArduinoJson.h>
#include <WiFi.h>

#define DEBUG_OPEN_METEO_API 1
#define ENABLE_CLOUD_MOCK 0 

// Przechowujemy obie serie danych: MSL oraz na poziomie gruntu
static float pressureMslHistoryData[12] = {0};
static float pressureSurfaceHistoryData[12] = {0};
static bool dataValid = false; 

// --- IMPLEMENTACJA GETTERÓW ---
const float* getOpenMeteoPressureHistory() {
    // Zwraca odpowiednią serię w zależności od wyboru użytkownika
    return showPressureAtSeaLevel ? pressureMslHistoryData : pressureSurfaceHistoryData;
}

const float* getOpenMeteoPressureMslHistory() {
    return pressureMslHistoryData;
}

const float* getOpenMeteoPressureSurfaceHistory() {
    return pressureSurfaceHistoryData;
}

bool isOpenMeteoDataValid() {
    return dataValid;
}

void setOpenMeteoPressureHistory(const float* mslData, const float* surfData) {
    for(int i = 0; i < 12; i++) {
        pressureMslHistoryData[i] = mslData[i];
        pressureSurfaceHistoryData[i] = surfData[i];
    }
    dataValid = true;
}
// ------------------------------

void fetchOpenMeteoPressure() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ [Open-Meteo] Brak WiFi. Używam starych danych z RAM (jeśli są).");
        return;
    }

    WeatherLocation currentLoc = locationManager.getCurrentLocation();
    WiFiClient client; 
    HTTPClient http;
    
    // ZMIANA: Pobieramy teraz zarówno pressure_msl (nad poziomem morza), jak i surface_pressure (na poziomie stacji)
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) + 
                 "&longitude=" + String(currentLoc.longitude, 4) + 
                 "&hourly=pressure_msl,surface_pressure&past_hours=11&forecast_hours=1&current=cloud_cover";
    
    Serial.printf("🌐 [Open-Meteo] Pobieranie historii ciśnienia i chmur dla: %s\n", currentLoc.displayName.c_str());

    http.begin(client, url); 
    http.setTimeout(4000); 
    yield(); 

    int httpCode = http.GET();
    yield(); 

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString(); 
        yield(); 
        
        #ifdef DEBUG_OPEN_METEO_API
            Serial.println("=== RAW JSON OPEN-METEO API ===");
            Serial.printf("Lokalizacja: %s (Lat: %.4f, Lon: %.4f)\n", currentLoc.displayName.c_str(), currentLoc.latitude, currentLoc.longitude);
            Serial.println("=== KONIEC RAW JSON OPEN-METEO ===");
        #endif
        
        JsonDocument filter;
        filter["hourly"]["pressure_msl"] = true;
        filter["hourly"]["surface_pressure"] = true;
        filter["current"]["cloud_cover"] = true;
        
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        yield(); 

        if (error) {
            Serial.print("❌ [Open-Meteo] Błąd parsowania JSON. Zachowuję stary wykres. Błąd: ");
            Serial.println(error.c_str());
        } else {
            // 1. ODCZYT ZACHMURZENIA
            JsonObject current = doc["current"];
            if (!current.isNull() && current["cloud_cover"].is<int>()) {
                int currentClouds = current["cloud_cover"].as<int>();
                
                #if ENABLE_CLOUD_MOCK == 1
                static int mockState = 0;
                int mockValues[] = {5, 20, 40, 95}; 
                currentClouds = mockValues[mockState];
                weather.icon = "01d"; 
                Serial.printf("\n🧪 [MOCK] Wymuszone zachmurzenie: %d%%\n", currentClouds);
                mockState++;
                if (mockState > 3) mockState = 0; 
                #endif

                Serial.printf("☁️ [Open-Meteo] Pobrano aktualne zachmurzenie: %d%%\n", currentClouds);
                
                weather.cloudiness = currentClouds; 

                if (weather.icon.length() >= 2) {
                    char prefix0 = weather.icon[0];
                    char prefix1 = weather.icon[1];
                    char suffix = (weather.icon.length() >= 3) ? weather.icon[2] : 'd';
                    
                    if (prefix0 == '0' && (prefix1 >= '1' && prefix1 <= '4')) {
                        if (currentClouds <= 10) {
                            weather.description = "clear sky";
                            weather.icon = String("01") + suffix;
                        } else if (currentClouds <= 30) {
                            weather.description = "few clouds";
                            weather.icon = String("02") + suffix;
                        } else if (currentClouds <= 70) {
                            weather.description = "scattered clouds";
                            weather.icon = String("03") + suffix;
                        } else if (currentClouds <= 84) {
                            weather.description = "broken clouds";
                            weather.icon = String("04") + suffix;
                        } else {
                            weather.description = "overcast clouds";
                            weather.icon = String("04") + suffix;
                        }
                        Serial.println("🔄 [Open-Meteo] Podmieniono opis na: " + weather.description + " (" + weather.icon + ")");
                    } else {
                        Serial.println("☔ [Open-Meteo] Zachmurzenie zaktualizowane, ale zachowuję ikonę opadów: " + weather.icon);
                    }
                }
            }

            // 2. ODCZYT OBU SERII CIŚNIENIA
            JsonArray mslArray = doc["hourly"]["pressure_msl"];
            JsonArray surfArray = doc["hourly"]["surface_pressure"];
            
            float tempMsl[12] = {0};
            float tempSurf[12] = {0};
            int countMsl = 0;
            int countSurf = 0;

            for (float p : mslArray) {
                if (countMsl < 12) {
                    tempMsl[countMsl] = p;
                    countMsl++;
                }
            }

            for (float p : surfArray) {
                if (countSurf < 12) {
                    tempSurf[countSurf] = p;
                    countSurf++;
                }
            }

            if (countMsl == 12 && countSurf == 12) {
                for(int i = 0; i < 12; i++) {
                    pressureMslHistoryData[i] = tempMsl[i];
                    pressureSurfaceHistoryData[i] = tempSurf[i];
                }
                dataValid = true; 
                
                // Aktualizujemy globalne ciśnienie w zależności od trybu
                weather.pressure = showPressureAtSeaLevel ? pressureMslHistoryData[11] : pressureSurfaceHistoryData[11];

                Serial.println("✅ [Open-Meteo] Pomyślnie zaktualizowano dane ciśnienia (MSL & Surface)!");
            } else {
                Serial.println("⚠️ [Open-Meteo] Otrzymano niekompletne dane ciśnienia. Zachowuję stary wykres.");
            }
        }
    } else {
        Serial.printf("❌ [Open-Meteo] Błąd sieci HTTP: %d. Zachowuję stary wykres w pamięci RAM.\n", httpCode);
    }
    
    http.end();
}