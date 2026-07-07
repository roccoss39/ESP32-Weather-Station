#include "weather/open_meteo_api.h"
#include "config/location_config.h" 
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static float pressureHistoryData[12] = {0};
static bool dataValid = false;

// --- IMPLEMENTACJA GETTERÓW ---
const float* getOpenMeteoPressureHistory() {
    return pressureHistoryData;
}

bool isOpenMeteoDataValid() {
    return dataValid;
}
// ------------------------------

void fetchOpenMeteoPressure() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ [Open-Meteo] Brak WiFi. Nie mogę pobrać historii.");
        dataValid = false;
        return;
    }

    WeatherLocation currentLoc = locationManager.getCurrentLocation();

    WiFiClientSecure client;
    client.setInsecure(); // Akceptujemy każdy certyfikat

    HTTPClient http;
    
    // Dynamicznie budujemy URL używając zmiennych z LocationManagera (z dokładnością do 4 miejsc po przecinku)
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) + 
                 "&longitude=" + String(currentLoc.longitude, 4) + 
                 "&hourly=surface_pressure&past_hours=11&forecast_hours=1";
    
    Serial.printf("🌐 [Open-Meteo] Pobieranie historii ciśnienia dla: %s (%.2f, %.2f)\n", 
                  currentLoc.displayName.c_str(), currentLoc.latitude, currentLoc.longitude);

    http.begin(client, url); 
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString(); 
        
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("❌ [Open-Meteo] Błąd parsowania JSON: ");
            Serial.println(error.c_str());
            dataValid = false;
        } else {
            JsonArray pressureArray = doc["hourly"]["surface_pressure"];
            
            int count = 0;
            for (float p : pressureArray) {
                if (count < 12) {
                    pressureHistoryData[count] = p;
                    count++;
                }
            }

            if (count == 12) {
                dataValid = true;
                Serial.println("✅ [Open-Meteo] Pobrano 12h historii ciśnienia pomyślnie!");
            } else {
                Serial.println("⚠️ [Open-Meteo] Otrzymano niekompletną tablicę danych.");
                dataValid = false;
            }
        }
    } else {
        Serial.printf("❌ [Open-Meteo] Błąd HTTP: %d\n", httpCode);
        dataValid = false;
    }
    
    http.end();
}