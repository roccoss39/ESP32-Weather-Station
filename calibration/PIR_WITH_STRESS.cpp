#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define PIR_PIN 27

void setup() {
    Serial.begin(115200);
    pinMode(PIR_PIN, INPUT);
    WiFi.begin("Twoje_SSID", "Haslo"); // Musi próbować się łączyć!
}

void loop() {
    // --- STRESS TEST 1: WiFi Scan ---
    // Skanowanie sieci WiFi pobiera mnóstwo prądu i generuje szum EMI
    WiFi.scanNetworks(); 
    Serial.print("WiFi Stress... ");

    // --- STRESS TEST 2: PWM & Backlight ---
    // Szybkie przełączanie jasności ekranu (jeśli masz podpięty pin 25)
    for(int i=0; i<255; i+=50) {
        analogWrite(25, i);
        delay(10);
    }

    // --- MONITOROWANIE PIR ---
    int state = digitalRead(PIR_PIN);
    Serial.printf("PIR State: %s\n", state == HIGH ? "!!! RUCH !!!" : "ok");

    delay(100); 
}