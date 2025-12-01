#include "display/github_image.h"
#include "config/display_config.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

#define TEST_IMG 0

// --- ZMIENNE GLOBALNE ---
CurrentImageData currentImage;

// --- INCLUDE ULTIMATE NASA COLLECTION (1359 obrazków) ---
#include "photo_display/esp32_nasa_ultimate.h"

// --- RANDOM CONFIG: wszystkie obrazki ---
const unsigned long IMAGE_CHANGE_INTERVAL = 3000;  // 3 sekundy

// Callback dla TJpg_Decoder (z photo_display) - Z DEBUG
bool tft_output_nasa(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  extern TFT_eSPI tft;
  
  static int callbackCount = 0;
  callbackCount++;
  
  // Debug pierwszych 5 wywołań
  if (callbackCount <= 5) {
    Serial.printf("🎨 NASA Callback #%d: x=%d, y=%d, w=%d, h=%d\n", callbackCount, x, y, w, h);
  }
  
  if (y >= 240) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// Funkcja ładowania fallback image z SPIFFS
bool loadFallbackImageFromSPIFFS() {
  extern TFT_eSPI tft;
  
  Serial.println("🛡️ Ładuję fallback image z SPIFFS...");
  
  // Sprawdź czy SPIFFS jest zamountowany
  if (!SPIFFS.begin()) {
    Serial.println("❌ SPIFFS mount failed");
    return false;
  }
  
  // Sprawdź czy plik istnieje
  if (!SPIFFS.exists("/fallback_error_img.jpg")) {
    Serial.println("❌ Fallback image nie istnieje w SPIFFS");
    return false;
  }
  
  // Otwórz plik
  File file = SPIFFS.open("/fallback_error_img.jpg", "r");
  if (!file) {
    Serial.println("❌ Nie można otworzyć fallback image");
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.println("📦 Fallback image size: " + String(fileSize) + " bytes");
  
  // Alokuj buffer
  uint8_t* buffer = (uint8_t*)malloc(fileSize);
  if (!buffer) {
    Serial.println("❌ Brak pamięci dla fallback image");
    file.close();
    return false;
  }
  
  // Czytaj plik
  file.read(buffer, fileSize);
  file.close();
  
  // Wyczyść ekran
  tft.fillScreen(TFT_BLACK);
  
  // Setup TJpg_Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output_nasa);
  
  // Dekoduj fallback image
  int result = TJpgDec.drawJpg(0, 0, buffer, fileSize);
  
  if (result == 0) {
    Serial.println("✅ Fallback image załadowany pomyślnie!");
    
    // Dodaj napis "FALLBACK IMAGE"
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("FALLBACK IMAGE - NASA CONNECTION ERROR", tft.width() / 2, 230);
    
    free(buffer);
    return true;
  } else {
    Serial.println("❌ Fallback image decode error: " + String(result));
    free(buffer);
    return false;
  }
}

void initNASAImageSystem() {
  Serial.println("=== INICJALIZACJA NASA ULTIMATE SYSTEM ===");
  Serial.println("Calkowita kolekcja: " + String(num_nasa_images) + " obrazków");
  Serial.println("RANDOM MODE: Losowe obrazki co " + String(IMAGE_CHANGE_INTERVAL/1000) + " sekund");
  
  // Zainicjalizuj pierwszy obraz (z pierwszych 3 dla testów)
  currentImage.imageNumber = 0; // Zacznij od pierwszego obrazka
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename); // filename zamiast date
  currentImage.isValid = false;
  currentImage.lastUpdate = 0;
  
  Serial.println("NASA Ultimate System gotowy!");
  Serial.println("GitHub URLs: roccoss39.github.io/nasa.github.io-/");
}

bool getRandomNASAImage() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Brak WiFi - nie można pobrać zdjęcia NASA");
    return false;
  }
  
  // LOSOWY WYBÓR ze wszystkich 1359 obrazków
  currentImage.imageNumber = random(0, num_nasa_images); // 0-1358 (losowy)
  
  if (TEST_IMG == 1)
  {
   Serial.println("podmieniam");
   currentImage.url = "https://roccoss39.github.io/nasa.github.io-/nasa-images/Colorful_Airglow_Bands_Surround_Milky_Way.jpg";
  }
  else
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename); // filename as date
  currentImage.lastUpdate = millis();
  
  Serial.println("=== 🎲 RANDOM NASA " + String(currentImage.imageNumber + 1) + "/" + String(num_nasa_images) + " (MEGA COLLECTION) ===");
  Serial.println("Tytuł: " + currentImage.title);
  Serial.println("URL: " + currentImage.url);
  
  currentImage.isValid = true;
  return true;
}

void displayGitHubImage(TFT_eSPI& tft) {
  Serial.println("=== EKRAN NASA ULTIMATE ===");
  
  // AUTO-ADVANCE: Zmiana obrazka co 3 sekundy
  static unsigned long lastImageChange = 0;
  static bool firstRun = true;
  
  if (firstRun || (millis() - lastImageChange >= IMAGE_CHANGE_INTERVAL)) {
    Serial.println("🔄 Zmiana obrazka NASA...");
    
    if (!getRandomNASAImage()) {
      // Nie udało się pobrać - pokaż błąd
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("BLAD NASA", tft.width() / 2, tft.height() / 2 - 20);
      tft.setTextSize(1);
      tft.drawString("Sprawdz polaczenie", tft.width() / 2, tft.height() / 2 + 10);
      return;
    }
    
    // Spróbuj załadować i wyświetlić nowy NASA obrazek
    if (downloadAndDisplayImage(tft, currentImage.imageNumber)) {
      Serial.println("✅ NASA obraz wyświetlony pomyślnie!");
      lastImageChange = millis();
      firstRun = false;
    } else {
      Serial.println("❌ Nie udało się wyświetlić NASA obrazka - próbuję inny...");
      
      // RETRY: Spróbuj 2 razy z różnymi obrazkami
      bool retrySuccess = false;
      for (int retry = 0; retry < 2 && !retrySuccess; retry++) {
        int retryImage = random(0, min(50, num_nasa_images)); // Pierwsze 50 obrazków (najbardziej stabilne)
        Serial.println("🔄 RETRY " + String(retry + 1) + "/2: NASA #" + String(retryImage + 1));
        
        if (downloadAndDisplayImage(tft, retryImage)) {
          Serial.println("✅ RETRY sukces!");
          currentImage.imageNumber = retryImage;
          lastImageChange = millis();
          firstRun = false;
          retrySuccess = true;
        }
      }
      
      if (!retrySuccess) {
        Serial.println("❌ Wszystkie RETRY nieudane - próbuję fallback image");
        
        // Spróbuj załadować fallback image z SPIFFS
        if (loadFallbackImageFromSPIFFS()) {
          Serial.println("✅ Fallback image załadowany - problem rozwiązany!");
          firstRun = false;
          lastImageChange = millis();
          return; // Sukces - zakończ funkcję
        }
        
        Serial.println("❌ Fallback image też nie działa - pokazuję error screen");
      }
      
      // Jeśli wszystko zawiedzie, pokaż tekstowy error
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("CRITICAL ERROR", tft.width() / 2, tft.height() / 2 - 30);
      tft.setTextSize(1);
      tft.drawString("NASA Connection Failed", tft.width() / 2, tft.height() / 2 - 10);
      tft.drawString("Fallback Image Failed", tft.width() / 2, tft.height() / 2 + 10);
      tft.drawString("Check SPIFFS & Internet", tft.width() / 2, tft.height() / 2 + 30);
    }
  }
}

bool downloadAndDisplayImage(TFT_eSPI& tft, int imageIndex) {
  if (imageIndex >= num_nasa_images || imageIndex < 0) return false;
  
  
  NASAImage selectedImage = nasa_ultimate_collection[imageIndex];
  
  Serial.println("=== NASA Image " + String(imageIndex + 1) + " ===");
  Serial.println("Title: " + String(selectedImage.title));
  Serial.println("Filename: " + String(selectedImage.filename));
  Serial.println("URL: " + String(selectedImage.url));
  
  // Pokaż loading screen z ikoną ładowania
  tft.fillScreen(TFT_BLACK);
  
  // Ikona ładowania - animowany spinner
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  
  // Narysuj spinner (kółko z linią)
  tft.drawCircle(centerX, centerY, 20, TFT_CYAN);
  tft.drawCircle(centerX, centerY, 15, TFT_BLUE);
  tft.drawLine(centerX, centerY, centerX + 15, centerY - 10, TFT_WHITE);
  
  // Bez tekstu - tylko ikona ładowania
  
  // HTTP download przez HTTPClient (prostsze niż WiFiClientSecure)
  HTTPClient http;

    if (TEST_IMG == 1)
  {
   Serial.println("podmieniam");
   selectedImage.url = "https://roccoss39.github.io/nasa.github.io-/nasa-images/Colorful_Airglow_Bands_Surround_Milky_Way@@@@@.jpg";
  }


  http.begin(selectedImage.url);
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ HTTP Error: " + String(httpCode));
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Download Failed!", 10, 100);
    tft.drawString("HTTP: " + String(httpCode), 10, 120);
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  Serial.println("File size: " + String(contentLength) + " bytes");
  
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("❌ No stream available");
    http.end();
    return false;
  }
  
  // Download to buffer
  uint8_t* buffer = (uint8_t*)malloc(contentLength);
  if (!buffer) {
    Serial.println("❌ Memory allocation failed");
    http.end();
    return false;
  }
  
  stream->readBytes(buffer, contentLength);
  
  // Clear and display image
  tft.fillScreen(TFT_BLACK);
  
  // Setup TJpgDecoder with RGB swap for correct colors
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);  // Fix purple/violet colors
  TJpgDec.setCallback(tft_output_nasa);
  
  // DODAJ DEBUGOWANIE PRZED DEKODOWANIEM
  Serial.printf("📊 Buffer info: %d bytes, first 4 bytes: %02X %02X %02X %02X\n", 
                contentLength, buffer[0], buffer[1], buffer[2], buffer[3]);
  
  // Sprawdź czy to prawdziwy JPEG (powinien zaczynać się od FF D8)
  if (contentLength < 4 || buffer[0] != 0xFF || buffer[1] != 0xD8) {
    Serial.println("❌ BŁĄD: To nie jest poprawny JPEG!");
    Serial.printf("Expected FF D8, got %02X %02X\n", buffer[0], buffer[1]);
    free(buffer);
    http.end();
    return false;
  }
  
  int result = TJpgDec.drawJpg(0, 0, buffer, contentLength);
  
  if (result == 0) {
    // Dodaj tytuł na dole - wyśrodkowany
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM); // Wyśrodkowanie
    
    String titleStr = String(selectedImage.title);
    if (titleStr.length() > 45) {
      titleStr = titleStr.substring(0, 42) + "...";
    }
    
    // Wyśrodkowany na dole ekranu
    tft.drawString(titleStr, tft.width() / 2, 225);
    
    // Usuń progress info - tylko czysty obrazek
    
    Serial.println("✅ NASA image displayed successfully!");
    free(buffer);
    http.end();
    return true;
  } else {
    // LEPSZE DEBUGOWANIE BŁĘDÓW JPEG
    String errorMsg;
    switch(result) {
      case 1: errorMsg = "Interrupted by output function"; break;
      case 2: errorMsg = "Device error or wrong termination"; break;
      case 3: errorMsg = "Insufficient memory pool"; break;
      case 4: errorMsg = "Insufficient stream input buffer"; break;
      case 5: errorMsg = "Parameter error"; break;
      case 6: errorMsg = "Data format error (not JPEG file)"; break;
      case 7: errorMsg = "Right format but not supported"; break;
      case 8: errorMsg = "Not supported JPEG standard"; break;
      default: errorMsg = "Unknown error"; break;
    }
    
    Serial.println("✗ JPEG decode error: " + String(result) + " - " + errorMsg);
    Serial.println("📄 URL: " + String(selectedImage.url));
    Serial.println("📦 Content-Length: " + String(contentLength));
    
    // FALLBACK: Pokaż error screen z detalami
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("JPEG ERROR " + String(result), 10, 10);
    tft.drawString(errorMsg, 10, 30);
    tft.drawString("Size: " + String(contentLength) + " bytes", 10, 50);
    tft.drawString("NASA #" + String(imageIndex + 1), 10, 70);
    
    free(buffer);
    http.end();
    return false;
  }
}