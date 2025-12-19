#ifndef GITHUB_UPDATE_MANAGER_H
#define GITHUB_UPDATE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
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
        
        // Klient bezpieczny (HTTPS)
        WiFiClientSecure client;
        client.setInsecure(); // Ignorujemy certyfikaty (łatwiejsze, choć mniej bezpieczne)
        // Jeśli chcesz pełne bezpieczeństwo, musiałbyś wgrać certyfikat Root CA GitHuba

        // Konfiguracja HTTP Update
        httpUpdate.setLedPin(LED_STATUS_PIN, LOW); // Mrugaj diodą przy pobieraniu
        
        // Callbacki (co robić w trakcie)
        httpUpdate.onStart(update_started);
        httpUpdate.onEnd(update_finished);
        httpUpdate.onProgress(update_progress);
        httpUpdate.onError(update_error);

        // === PRÓBA AKTUALIZACJI ===
        // Ta funkcja sama pobierze, sprawdzi i zrestartuje ESP jeśli się uda!
        // Uwaga: Można tu dodać logikę sprawdzania wersji w pliku tekstowym przed pobraniem .bin,
        // ale dla uproszczenia - HTTPUpdate po prostu spróbuje pobrać plik.
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
    }

private:
    static void update_started() {
        Serial.println("⬇️ ROZPOCZYNAM POBIERANIE FIRMWARE...");
    }
    static void update_finished() {
        Serial.println("\n✅ POBIERANIE ZAKOŃCZONE. Restart...");
    }
    static void update_progress(int cur, int total) {
        Serial.printf("⏳ Postęp: %d%%\r", (cur * 100) / total);
    }
    static void update_error(int err) {
        Serial.printf("❌ Błąd OTA: %d\n", err);
    }
};

#endif