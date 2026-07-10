#include "weather/open_meteo_api.h"
#include "config/location_config.h" 
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>

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
// ------------------------------

void fetchOpenMeteoPressure() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ [Open-Meteo] Brak WiFi. Używam starych danych z RAM (jeśli są).");
        // Celowo NIE zmieniamy dataValid na false, aby stary wykres nie zniknął z ekranu
        return;
    }

    // Pobieranie współrzędnych z Twojego menedżera lokalizacji
    WeatherLocation currentLoc = locationManager.getCurrentLocation();

    WiFiClientSecure client;
    client.setInsecure(); // Akceptujemy każdy certyfikat (bezpieczne dla zapytań o pogodę)

    HTTPClient http;
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(currentLoc.latitude, 4) + 
                 "&longitude=" + String(currentLoc.longitude, 4) + 
                 "&hourly=surface_pressure&past_hours=11&forecast_hours=1";
    
    Serial.printf("🌐 [Open-Meteo] Pobieranie historii ciśnienia dla: %s\n", currentLoc.displayName.c_str());

    http.begin(client, url); 
    
    // ZMIANA 1: Zmniejszono z 8000 na 4000 ms. 
    // Zmuszamy HTTP do poddania się po 4s, co uchroni nas przed Watchdogiem (który reaguje po 5s).
    http.setTimeout(4000); 
    
    yield(); // ZMIANA 2: Karmimy psa przed zawieszeniem się na pobieraniu

    int httpCode = http.GET();

    yield(); // ZMIANA 3: Karmimy psa natychmiast po pobraniu!

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString(); 
        yield(); // Kolejny oddech dla procesora
        
        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, payload);

        yield(); // Oddech po parsowaniu JSONa

        if (error) {
            Serial.print("❌ [Open-Meteo] Błąd parsowania JSON. Zachowuję stary wykres. Błąd: ");
            Serial.println(error.c_str());
        } else {
            JsonArray pressureArray = doc["hourly"]["surface_pressure"];
            
            // Bezpieczny bufor. Wpisujemy najpierw tu, żeby nie zepsuć głównych danych
            // w przypadku, gdyby tablica w JSON była niekompletna.
            float tempArray[12] = {0};
            int count = 0;
            for (float p : pressureArray) {
                if (count < 12) {
                    tempArray[count] = p;
                    count++;
                }
            }

            if (count == 12) {
                // Skoro mamy idealne nowe 12 odczytów, podmieniamy stare:
                for(int i = 0; i < 12; i++) {
                    pressureHistoryData[i] = tempArray[i];
                }
                dataValid = true; // Od teraz wykres jest zawsze gotowy do wyświetlenia
                Serial.println("✅ [Open-Meteo] Pomyślnie zaktualizowano dane na wykresie!");
            } else {
                Serial.println("⚠️ [Open-Meteo] Otrzymano niekompletne dane. Zachowuję stary wykres.");
            }
        }
    } else {
        Serial.printf("❌ [Open-Meteo] Błąd sieci HTTP: %d. Zachowuję stary wykres w pamięci RAM.\n", httpCode);
    }
    
    http.end();
}