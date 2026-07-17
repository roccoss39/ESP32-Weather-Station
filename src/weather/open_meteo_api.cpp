#include "weather/open_meteo_api.h"
#include "config/location_config.h" 
#include "weather/weather_data.h"
#include <HTTPClient.h>
#include <WiFiClient.h> 
#include <ArduinoJson.h>
#include <WiFi.h>

// --- DODANY PRZEŁĄCZNIK DEBUGOWANIA ---
#define DEBUG_OPEN_METEO_API 1
#define ENABLE_CLOUD_MOCK 0 // 1 = Włączony test ikon. Zmień na 0 po zakończeniu testów

static float pressureHistoryData[12] = {0};
static bool dataValid = false; 

// --- IMPLEMENTACJA GETTERÓW ---
const float* getOpenMeteoPressureHistory() {
    return pressureHistoryData;
}

bool isOpenMeteoDataValid() {
    return dataValid;
}

void setOpenMeteoPressureHistory(const float* data) {
    for(int i = 0; i < 12; i++) {
        pressureHistoryData[i] = data[i];
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
    
    // Pobieramy ciśnienie i zachmurzenie (cloud_cover)
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) + 
                 "&longitude=" + String(currentLoc.longitude, 4) + 
                 "&hourly=pressure_msl&past_hours=11&forecast_hours=1&current=cloud_cover";
    
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
        
        // ==============================================================
        // TARCZA OCHRONNA RAM: Filtr JSON
        // ==============================================================
        JsonDocument filter;
        filter["hourly"]["pressure_msl"] = true;
        filter["current"]["cloud_cover"] = true;
        
        JsonDocument doc; 
        // Deserializujemy TYLKO to, co zdefiniowaliśmy w filtrze wyżej!
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        yield(); 

        if (error) {
            Serial.print("❌ [Open-Meteo] Błąd parsowania JSON. Zachowuję stary wykres. Błąd: ");
            Serial.println(error.c_str());
        } else {
            // ---------------------------------------------------------
            // 1. ODCZYT ZACHMURZENIA I BEZPIECZNA ZMIANA IKON
            // ---------------------------------------------------------
            JsonObject current = doc["current"];
            if (!current.isNull() && current["cloud_cover"].is<int>()) {
                int currentClouds = current["cloud_cover"].as<int>();
                
                // ==========================================
                // TRYB MOCK - TESTOWANIE IKON
                // ==========================================
                #if ENABLE_CLOUD_MOCK == 1
                static int mockState = 0;
                int mockValues[] = {5, 20, 40, 95}; // Odpowiada progom: <10, <25, <50, >84
                currentClouds = mockValues[mockState];
                weather.icon = "01d"; // Wymuszamy bazę słońca, żeby mock działał nawet gdy pada deszcz
                Serial.printf("\n🧪 [MOCK] Wymuszone zachmurzenie: %d%%\n", currentClouds);
                mockState++;
                if (mockState > 3) mockState = 0; // Zapętl na 4 ikonie
                #endif
                // ==========================================

                Serial.printf("☁️ [Open-Meteo] Pobrano aktualne zachmurzenie: %d%%\n", currentClouds);
                
                weather.cloudiness = currentClouds; 

                // Bezpieczna manipulacja znakami char (chroni przed fragmentacją RAM)
                if (weather.icon.length() >= 2) {
                    char prefix0 = weather.icon[0];
                    char prefix1 = weather.icon[1];
                    char suffix = (weather.icon.length() >= 3) ? weather.icon[2] : 'd';
                    
                    if (prefix0 == '0' && (prefix1 >= '1' && prefix1 <= '4')) {
                        if (currentClouds <= 10) {
                            weather.description = "clear sky";
                            weather.icon = String("01") + suffix;
                        } else if (currentClouds <= 25) {
                            weather.description = "few clouds";
                            weather.icon = String("02") + suffix;
                        } else if (currentClouds <= 50) {
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

            // ---------------------------------------------------------
            // 2. ODCZYT WYKRESU CIŚNIENIA
            // ---------------------------------------------------------
            JsonArray pressureArray = doc["hourly"]["pressure_msl"];
            
            float tempArray[12] = {0};
            int count = 0;
            for (float p : pressureArray) {
                if (count < 12) {
                    tempArray[count] = p;
                    count++;
                }
            }

            if (count == 12) {
                for(int i = 0; i < 12; i++) {
                    pressureHistoryData[i] = tempArray[i];
                }
                dataValid = true; 
                Serial.println("✅ [Open-Meteo] Pomyślnie zaktualizowano dane ciśnienia!");
            } else {
                Serial.println("⚠️ [Open-Meteo] Otrzymano niekompletne dane ciśnienia. Zachowuję stary wykres.");
            }
        }
    } else {
        Serial.printf("❌ [Open-Meteo] Błąd sieci HTTP: %d. Zachowuję stary wykres w pamięci RAM.\n", httpCode);
    }
    
    http.end();
}