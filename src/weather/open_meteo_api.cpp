#include "weather/open_meteo_api.h"
#include "config/location_config.h" 
#include "weather/weather_data.h"
#include <HTTPClient.h>
#include <WiFiClient.h> // Zwykły, super-lekki klient WiFi
#include <ArduinoJson.h>
#include <WiFi.h>

// --- DODANY PRZEŁĄCZNIK DEBUGOWANIA ---
#define DEBUG_OPEN_METEO_API 1

static float pressureHistoryData[12] = {0};
// Flaga na starcie jest false. Gdy raz pobierze dane, staje się na stałe true!
static bool dataValid = false; 

// --- IMPLEMENTACJA GETTERÓW ---
const float* getOpenMeteoPressureHistory() {
    return pressureHistoryData;
}

bool isOpenMeteoDataValid() {
    return dataValid;
}

// --- FUNKCJA ODTWARZAJĄCA Z PAMIĘCI RTC ---
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

    // Używamy nieszyfrowanego klienta - odzyskujemy ~30KB pamięci RAM!
    WiFiClient client; 

    HTTPClient http;
    // URL tylko dla ciśnienia, bez pobierania chmur (zabezpieczenie przed brakiem pamięci)
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) + 
                 "&longitude=" + String(currentLoc.longitude, 4) + 
                 "&hourly=pressure_msl&past_hours=11&forecast_hours=1";
    
    Serial.printf("🌐 [Open-Meteo] Pobieranie historii ciśnienia dla: %s\n", currentLoc.displayName.c_str());

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
            // Serial.println(payload); // Zakomentowane, by nie śmiecić logów
            Serial.println("=== KONIEC RAW JSON OPEN-METEO ===");
        #endif
        
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload);
        yield(); 

        if (error) {
            Serial.print("❌ [Open-Meteo] Błąd parsowania JSON. Zachowuję stary wykres. Błąd: ");
            Serial.println(error.c_str());
        } else {
            // ---------------------------------------------------------
            // ODCZYT WYKRESU CIŚNIENIA
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