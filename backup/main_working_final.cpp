#include <Arduino.h>
#include <WiFi.h>
#include <time.h>       // Standardowa biblioteka C do obsługi czasu
#include <SPI.h>
#include <TFT_eSPI.h>

// --- USTAWIENIA UŻYTKOWNIKA ---
const char* ssid = "zero"; // Wpisz nazwę swojej sieci Wi-Fi
const char* password = "Qweqweqwe1"; // Wpisz hasło do Wi-Fi

const char* ntpServer = "pool.ntp.org";
// Ustawienie strefy czasowej dla Polski (CET/CEST)
// Format: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// CET-1CEST,M3.5.0/2,M10.5.0/3 = Czas środkowoeuropejski, zmiana czasu letniego
// zaczyna się w ostatnią niedzielę marca o 2:00, kończy w ostatnią niedzielę października o 3:00
const char* tzInfo = "CET-1CEST,M3.5.0/2,M10.5.0/3";

TFT_eSPI tft = TFT_eSPI();

// Zmienne do przechowywania poprzedniego czasu (aby uniknąć migotania)
char timeStrPrev[9] = "        "; // 8 spacji na HH:MM:SS

void setup() {
  Serial.begin(115200);
  Serial.println("Zegar NTP na TFT");

  // --- Inicjalizacja TFT ---
  tft.init();
  tft.setRotation(1); // Ustaw orientację (np. 1)
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Kolor tekstu żółty, tło czarne
  tft.setTextDatum(MC_DATUM); // Ustaw punkt odniesienia tekstu na środek (Middle Center)
  tft.setTextSize(3); // Ustaw większy rozmiar czcionki (eksperymentuj)
  // Można też wybrać konkretną czcionkę, jeśli są załadowane w User_Setup:
  // tft.setFreeFont(&FreeSansBoldOblique24pt7b);

  tft.drawString("Laczenie z WiFi...", tft.width() / 2, tft.height() / 2);
  Serial.print("Laczenie z WiFi: ");
  Serial.println(ssid);

  // --- Łączenie z WiFi ---
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nPolaczono z WiFi");
  Serial.print("Adres IP: ");
  Serial.println(WiFi.localIP());

  // --- Konfiguracja czasu z NTP ---
  Serial.println("Konfiguracja czasu z NTP...");
  configTime(0, 0, ntpServer); // Starszy sposób, użyjemy configTzTime
  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Bez strefy czasowej
  configTzTime(tzInfo, ntpServer); // Użyj strefy czasowej z tzInfo
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Nie udalo sie pobrac czasu z NTP");
    tft.fillScreen(TFT_RED); // Sygnalizuj błąd
    tft.drawString("Blad NTP!", tft.width() / 2, tft.height() / 2);
    return; // Zatrzymaj, jeśli nie ma czasu
  }
  Serial.println("Czas pobrany i zsynchronizowany");
  tft.fillScreen(TFT_BLACK); // Wyczyść ekran po konfiguracji

  // Opcjonalnie: Rozłącz WiFi po synchronizacji (oszczędność energii)
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);
  // Serial.println("WiFi rozlaczone.");
}

void loop() {
  struct tm timeinfo; // Struktura do przechowywania informacji o czasie

  // Pobierz aktualny czas lokalny
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Blad odczytu czasu lokalnego");
    // Można dodać logikę ponownej synchronizacji NTP
    delay(1000);
    return;
  }

  // Sformatuj czas do postaci HH:MM:SS
  char timeStr[9]; // Bufor na HH:MM:SS + null terminator
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);

  // Sformatuj datę (opcjonalnie)
  char dateStr[11]; // Bufor na DD.MM.YYYY
  strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);

  // --- Rysowanie na ekranie ---

  // Optymalizacja: Rysuj tylko jeśli czas się zmienił (co sekundę)
  if (strcmp(timeStr, timeStrPrev) != 0) {
    // Wyczyść *tylko obszar* poprzedniego czasu, aby uniknąć migotania całego ekranu
    tft.fillRect(0, tft.height()/2 - 20, tft.width(), 40, TFT_BLACK); // Dostosuj wysokość i pozycję Y

    // Narysuj aktualny czas na środku ekranu (pionowo)
    tft.drawString(timeStr, tft.width() / 2, tft.height() / 2);

    // Narysuj datę poniżej (opcjonalnie)
    tft.setTextSize(2); // Mniejsza czcionka dla daty
    tft.drawString(dateStr, tft.width() / 2, tft.height() / 2 + 30); // Trochę niżej
    tft.setTextSize(3); // Wróć do większej czcionki dla czasu

    // Zapisz aktualny czas jako poprzedni do następnego porównania
    strcpy(timeStrPrev, timeStr);

    // Wyświetl też w Serial Monitorze (dla debugowania)
    Serial.print("Aktualny czas: ");
    Serial.print(dateStr);
    Serial.print(" ");
    Serial.println(timeStr);
  }

  delay(100); // Krótkie opóźnienie, aby nie obciążać procesora w pętli
}