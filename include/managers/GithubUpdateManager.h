#ifndef GITHUB_UPDATE_MANAGER_H
#define GITHUB_UPDATE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <esp_task_wdt.h> // <--- DODANO: Biblioteka Watchdoga
#include "config/hardware_config.h"
#include "config/secrets.h"

class GithubUpdateManager {
public:
    // Uruchom proces aktualizacji
    void checkForUpdate() {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("❌ Brak WiFi - nie mogę sprawdzić aktualizacji");
            return;
        }

        Serial.println("🔄 Sprawdzam aktualizacje na GitHub...");
        
        // === FIX WATCHDOG: Wydłużamy czas timeoutu na 60 sekund na czas update'u ===
        // SSL Handshake i pobieranie mogą chwilę potrwać
        esp_task_wdt_init(60, true); 
        esp_task_wdt_add(NULL); // Upewniamy się, że obecny wątek jest monitorowany

        // Klient bezpieczny (HTTPS)
        WiFiClientSecure client;
        client.setInsecure(); // Ignorujemy certyfikaty
        
        // Konfiguracja HTTP Update
        httpUpdate.setLedPin(LED_STATUS_PIN, LOW); 
        httpUpdate.rebootOnUpdate(true); // Restartuj po sukcesie
        
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
                break;
            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("✅ Brak nowych aktualizacji.");
                break;
            case HTTP_UPDATE_OK:
                Serial.println("✅ AKTUALIZACJA ZAKOŃCZONA SUKCESEM!");
                break;
        }
        
        // Po wszystkim (jeśli nie było resetu) przywracamy standardowy watchdog (opcjonalne, bo reboot i tak wyczyści)
        esp_task_wdt_init(5, true);
    }

private:
    static void update_started() {
        Serial.println("⬇️ ROZPOCZYNAM POBIERANIE FIRMWARE...");
    }

    static void update_finished() {
        Serial.println("\n✅ POBIERANIE ZAKOŃCZONE. Restart...");
    }

    static void update_progress(int cur, int total) {
        // Wyświetlaj kropkę co jakiś czas, żeby nie zalewać logów, albo procenty
        static int lastPercent = -1;
        int percent = (cur * 100) / total;
        
        if (percent != lastPercent) {
            Serial.printf("⏳ Postęp: %d%%\r", percent);
            lastPercent = percent;
        }
        
        // === FIX WATCHDOG: Karmimy psa w trakcie pobierania! ===
        esp_task_wdt_reset(); 
    }

    static void update_error(int err) {
        Serial.printf("❌ Błąd OTA: %d\n", err);
    }
};

#endif