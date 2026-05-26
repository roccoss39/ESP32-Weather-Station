#ifndef GITHUB_UPDATE_MANAGER_H
#define GITHUB_UPDATE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h> 
#include "config/hardware_config.h"
#include "config/secrets.h"

class GithubUpdateManager {
public:
    // Główna funkcja wywoływana z main.cpp
    void checkForUpdate() {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("❌ Brak WiFi - nie mogę sprawdzić aktualizacji");
            return;
        }

        // === KROK 1: Sprawdzenie wersji (Lekki plik tekstowy) ===
        Serial.println("🔍 Sprawdzanie dostępności nowej wersji (version.txt)...");

        // Definicja URL do pliku wersji (musi być RAW)
        // Upewnij się, że ten plik istnieje na GitHubie
        String versionUrl = "https://raw.githubusercontent.com/roccoss39/ESP32-Weather-Station/main/version.txt";

        WiFiClientSecure client;
        client.setInsecure(); // Ignorujemy certyfikaty (wymagane dla GitHub)

        HTTPClient http;
        
        // Rozszerzamy Watchdog na czas sprawdzania
        esp_task_wdt_init(60, true);
        esp_task_wdt_add(NULL);

        if (http.begin(client, versionUrl)) {
            int httpCode = http.GET();

            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                payload.trim(); // Usuwamy spacje i znaki nowej linii
                
                float remoteVersion = payload.toFloat();
                
                // Wypisz wersje dla debugowania
                Serial.printf("☁️ Wersja na GitHub: %.2f\n", remoteVersion);
                Serial.printf("🏠 Obecna wersja:   %.2f\n", FIRMWARE_VERSION); // FIRMWARE_VERSION musi być w hardware_config.h

                if (remoteVersion > FIRMWARE_VERSION) {
                    Serial.println("🚀 ZNALEZIONO NOWĄ WERSJĘ! Uruchamiam aktualizację...");
                    http.end(); // Zamykamy połączenie HTTPClient
                    
                    // === KROK 2: Właściwa aktualizacja (Ciężki plik .bin) ===
                    performUpdate(); 
                } else {
                    Serial.println("✅ Oprogramowanie jest aktualne. Pomijam pobieranie.");
                }
            } else {
                Serial.printf("❌ Błąd pobierania wersji: %s\n", http.errorToString(httpCode).c_str());
            }
            http.end();
        } else {
            Serial.println("❌ Nie udało się połączyć z serwerem wersji.");
        }
        
        // Reset Watchdoga po wszystkim
        esp_task_wdt_init(5, true);
    }

private:
    // Funkcja wykonująca właściwą aktualizację OTA (stary kod przeniesiony tutaj)
    void performUpdate() {
        Serial.println("🔄 Rozpoczynam pobieranie firmware.bin z GitHub...");
        
        // Klient bezpieczny (HTTPS) dla httpUpdate
        WiFiClientSecure client;
        client.setInsecure(); 
        
        // Konfiguracja HTTP Update
        uint8_t ledPin = getStatusLedPin();
        if (ledPin != 255) {
            httpUpdate.setLedPin(ledPin, LOW);
        } 
        httpUpdate.rebootOnUpdate(true); 
        
        // Callbacki
        httpUpdate.onStart(update_started);
        httpUpdate.onEnd(update_finished);
        httpUpdate.onProgress(update_progress);
        httpUpdate.onError(update_error);

        // === PRÓBA AKTUALIZACJI ===
        // GITHUB_FIRMWARE_URL musi być zdefiniowane w hardware_config.h
        t_httpUpdate_return ret = httpUpdate.update(client, GITHUB_FIRMWARE_URL);

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                Serial.printf("❌ Aktualizacja nieudana: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("✅ Brak nowych aktualizacji (według nagłówków serwera).");
                break;
            case HTTP_UPDATE_OK:
                Serial.println("✅ AKTUALIZACJA ZAKOŃCZONA SUKCESEM!");
                break;
        }
    }

    static void update_started() {
        Serial.println("⬇️ ROZPOCZYNAM POBIERANIE FIRMWARE...");
    }

    static void update_finished() {
        Serial.println("\n✅ POBIERANIE ZAKOŃCZONE. Restart...");
    }

    static void update_progress(int cur, int total) {
        static int lastPercent = -1;
        int percent = (cur * 100) / total;
        
        if (percent != lastPercent) {
            Serial.printf("⏳ Postęp: %d%%\r", percent);
            lastPercent = percent;
        }
        
        // Karmimy psa w trakcie długiego pobierania
        esp_task_wdt_reset(); 
    }

    static void update_error(int err) {
        Serial.printf("❌ Błąd OTA: %d\n", err);
    }
    
    // Helper do pobrania pinu LED (jeśli nie masz tej funkcji globalnie, możesz ją usunąć lub zdefiniować)
    uint8_t getStatusLedPin() {
        #ifdef LED_STATUS_PIN
            return LED_STATUS_PIN;
        #else
            return 255;
        #endif
    }
};

#endif