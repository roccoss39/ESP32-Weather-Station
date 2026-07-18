#ifndef GITHUB_UPDATE_MANAGER_H
#define GITHUB_UPDATE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h> 
#include <TFT_eSPI.h> // Dodano
#include "config/hardware_config.h"
#include "config/secrets.h"

extern TFT_eSPI tft; // Obiekt TFT na pewno istnieje w main.cpp

class GithubUpdateManager {
public:
    // Singleton-like metoda dla bezpiecznego przechowywania flagi UI (unikamy błędów linkera)
    static bool& isInteractiveUI() {
        static bool interactive = false;
        return interactive;
    }

    // Główna funkcja wywoływana z main.cpp lub ui.cpp
    void checkForUpdate(bool interactive = false) {
        isInteractiveUI() = interactive;

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("❌ Brak WiFi - nie mogę sprawdzić aktualizacji");
            if (isInteractiveUI()) {
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_RED);
                tft.setTextDatum(MC_DATUM);
                tft.drawString("Brak polaczenia", 160, 100);
                tft.drawString("z WiFi!", 160, 140);
                delay(2000);
            }
            return;
        }

        // === KROK 1: Sprawdzenie wersji (Lekki plik tekstowy) ===
        Serial.println("🔍 Sprawdzanie dostępności nowej wersji (version.txt)...");

        if (isInteractiveUI()) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_YELLOW);
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(2);
            tft.drawString("Sprawdzam wersje", 160, 100);
            tft.drawString("na GitHub...", 160, 140);
        }

        // Definicja URL do pliku wersji (musi być RAW)
        // ZMIANA: Dodajemy ?t=millis() aby ominąć narzucony przez GitHuba 5-minutowy cache CDN!
        String versionUrl = "https://raw.githubusercontent.com/roccoss39/ESP32-Weather-Station/main/version.txt?t=" + String(millis());

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
                Serial.printf("🏠 Obecna wersja:   %.2f\n", FIRMWARE_VERSION); 

                // Dodajemy mały margines błędu (0.001) dla ułamków zmiennoprzecinkowych,
                // aby 1.20000004 nie było uznawane za większe od 1.19999999
                if (remoteVersion > (FIRMWARE_VERSION + 0.001)) {
                    Serial.println("🚀 ZNALEZIONO NOWĄ WERSJĘ! Uruchamiam aktualizację...");
                    
                    if (isInteractiveUI()) {
                        tft.fillScreen(TFT_BLACK);
                        tft.setTextColor(TFT_GREEN);
                        tft.drawString("Znaleziono nowa", 160, 100);
                        tft.drawString(String("wersje: ") + String(remoteVersion, 2), 160, 140);
                        delay(2000);
                    }

                    http.end(); 
                    
                    // === KROK 2: Właściwa aktualizacja (Ciężki plik .bin) ===
                    performUpdate(); 
                } else {
                    Serial.println("✅ Oprogramowanie jest aktualne. Pomijam pobieranie.");
                    if (isInteractiveUI()) {
                        tft.fillScreen(TFT_BLACK);
                        tft.setTextColor(TFT_GREEN);
                        tft.drawString("Stacja jest", 160, 100);
                        tft.drawString("w pelni aktualna!", 160, 140);
                        delay(2500);
                    }
                }
            } else {
                Serial.printf("❌ Błąd pobierania wersji: %s\n", http.errorToString(httpCode).c_str());
                if (isInteractiveUI()) {
                    tft.fillScreen(TFT_BLACK);
                    tft.setTextColor(TFT_RED);
                    tft.drawString("Blad polaczenia", 160, 100);
                    tft.drawString("HTTP: " + String(httpCode), 160, 140);
                    delay(2500);
                }
            }
            http.end();
        } else {
            Serial.println("❌ Nie udało się połączyć z serwerem wersji.");
            if (isInteractiveUI()) {
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_RED);
                tft.drawString("Brak odpowiedzi", 160, 100);
                tft.drawString("od serwera GitHub", 160, 140);
                delay(2500);
            }
        }
        
        // Reset Watchdoga po wszystkim
        esp_task_wdt_init(5, true);
    }

private:
    // Funkcja wykonująca właściwą aktualizację OTA
    void performUpdate() {
        Serial.println("🔄 Rozpoczynam pobieranie firmware.bin z GitHub...");
        
        if (isInteractiveUI()) {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_YELLOW);
            tft.setTextDatum(MC_DATUM);
            tft.drawString("POBIERANIE", 160, 60);
            tft.drawString("AKTUALIZACJI...", 160, 100);
        }

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
        t_httpUpdate_return ret = httpUpdate.update(client, GITHUB_FIRMWARE_URL);

        switch (ret) {
            case HTTP_UPDATE_FAILED:
                Serial.printf("❌ Aktualizacja nieudana: (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
                if (isInteractiveUI()) {
                    tft.fillScreen(TFT_BLACK);
                    tft.setTextColor(TFT_RED);
                    tft.drawString("Blad aktualizacji!", 160, 100);
                    tft.setTextSize(1);
                    tft.drawString(httpUpdate.getLastErrorString(), 160, 140);
                    delay(4000);
                }
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

            if (isInteractiveUI()) {
                tft.fillRect(10, 160, 300, 24, 0x0000); // Wyczyść stare dane paska
                tft.drawRect(10, 160, 300, 24, 0xFFFF); // Biała ramka
                if (percent > 0) {
                    tft.fillRect(12, 162, (percent * 296) / 100, 20, 0x07E0); // Pasek (TFT_GREEN)
                }
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(0xFFFF);
                tft.setTextSize(2);
                tft.drawString(String(percent) + "%", 160, 200);
            }
        }
        
        // Karmimy psa w trakcie długiego pobierania
        esp_task_wdt_reset(); 
    }

    static void update_error(int err) {
        Serial.printf("❌ Błąd OTA: %d\n", err);
    }
    
    uint8_t getStatusLedPin() {
        #ifdef LED_STATUS_PIN
            return LED_STATUS_PIN;
        #else
            return 255;
        #endif
    }
};

#endif