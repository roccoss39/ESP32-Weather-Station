#include "weather/open_meteo_api.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>
#include <WiFi.h>

static float pressureHistoryData[12] = {0};
static bool dataValid = false;

// TODO: Zmień na współrzędne pobierane z Twojego menedżera lokalizacji
const String LATITUDE = "53.4289";  // Szczecin
const String LONGITUDE = "14.553";

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

    // Dodajemy klienta zabezpieczonego (Secure)
    WiFiClientSecure client;
    client.setInsecure(); // Akceptujemy każdy certyfikat - bezpieczne dla pobierania pogody

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + LATITUDE + "&longitude=" + LONGITUDE + "&hourly=surface_pressure&past_hours=11&forecast_hours=1";
    
    Serial.println("🌐 [Open-Meteo] Pobieranie historii ciśnienia...");

    // Podajemy klienta zabezpieczonego jako pierwszy argument
    http.begin(client, url); 
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        // Zamiast czytać ze strumienia, pobieramy całość do tekstu
        String payload = http.getString(); 
        
        // Możesz odkomentować poniższą linię, aby zobaczyć surowe dane z internetu:
         Serial.println("📦 [Open-Meteo] Surowe dane: " + payload);

        JsonDocument doc; 
        // Parsujemy pobrany przed chwilą tekst
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