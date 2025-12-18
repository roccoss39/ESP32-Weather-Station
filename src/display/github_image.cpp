#include "display/github_image.h"
#include "config/display_config.h"
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <LittleFS.h> // <--- ZMIANA: LittleFS zamiast SPIFFS

#define TEST_ONE_IMG 0  // 0 = Normalny tryb losowy, 1 = Test jednego URL
#define DEBUG_IMAGES 0  // 0 = Mniej logów, 1 = Pełne logi debugowania

// --- ZMIENNE GLOBALNE ---
CurrentImageData currentImage;

// --- DEKLARACJA DISPLAYA (RAZ, NA GÓRZE) ---
extern TFT_eSPI tft;

// --- INCLUDE ULTIMATE NASA COLLECTION ---
#include "photo_display/esp32_nasa_ultimate.h"

// --- KONFIGURACJA CZASU ---
const unsigned long IMAGE_CHANGE_INTERVAL = 2000;  // Interwał zmiany (2 sekundy po zakończeniu poprzedniego)

// ================================================================
// CALLBACK DLA TJpg_Decoder
// ================================================================
bool tft_output_nasa(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Jeśli linia wychodzi poza ekran, przerwij
  if (y >= tft.height()) return 0;
  
  // Rysuj fragment obrazka
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// ================================================================
// FALLBACK: ŁADOWANIE Z LITTLEFS
// ================================================================
bool loadFallbackImageFromLittleFS() {
  Serial.println("🛡️ Ładuję fallback image z LittleFS...");
  
  // ZMIANA: LittleFS zamiast SPIFFS
  if (!LittleFS.begin()) {
    Serial.println("❌ LittleFS mount failed");
    return false;
  }
  
  // Sprawdź czy plik istnieje
  if (!LittleFS.exists("/fallback_error_img.jpg")) {
    Serial.println("❌ Fallback image nie istnieje w LittleFS");
    return false;
  }
  
  // Otwórz plik
  File file = LittleFS.open("/fallback_error_img.jpg", "r");
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

// ================================================================
// INICJALIZACJA SYSTEMU
// ================================================================
void initNASAImageSystem() {
  Serial.println("=== INICJALIZACJA NASA ULTIMATE SYSTEM ===");
  Serial.println("Calkowita kolekcja: " + String(num_nasa_images) + " obrazków");
  
  // Zainicjalizuj pierwszy obraz
  currentImage.imageNumber = 0; 
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename);
  currentImage.isValid = false;
  currentImage.lastUpdate = 0;
  
  Serial.println("NASA Ultimate System gotowy!");
}

// ================================================================
// WYBÓR LOSOWEGO OBRAZKA
// ================================================================
bool getRandomNASAImage() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Brak WiFi - nie można pobrać zdjęcia NASA");
    return false;
  }
  
  // LOSOWY WYBÓR ze wszystkich obrazków
  currentImage.imageNumber = random(0, num_nasa_images); 
  
  currentImage.url = String(nasa_ultimate_collection[currentImage.imageNumber].url);
  currentImage.title = String(nasa_ultimate_collection[currentImage.imageNumber].title);
  currentImage.date = String(nasa_ultimate_collection[currentImage.imageNumber].filename); 
  currentImage.lastUpdate = millis();
  
  Serial.println("=== 🎲 RANDOM NASA " + String(currentImage.imageNumber + 1) + "/" + String(num_nasa_images) + " ===");
  Serial.println("Tytuł: " + currentImage.title);
  Serial.println("URL: " + currentImage.url);
  
  currentImage.isValid = true;
  return true;
}

// ================================================================
// GŁÓWNA PĘTLA WYŚWIETLANIA (Logic Controller)
// ================================================================
void displayGitHubImage(TFT_eSPI& tft) {
  // Serial.println("=== EKRAN NASA ULTIMATE ==="); // Opcjonalnie, żeby nie spamować logów
  
  static unsigned long lastImageChange = 0;
  static bool firstRun = true;
  
  if (firstRun || (millis() - lastImageChange >= IMAGE_CHANGE_INTERVAL)) {
    Serial.println("🔄 Zmiana obrazka NASA...");
    
    if (!getRandomNASAImage()) {
      tft.fillScreen(COLOR_BACKGROUND);
      tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
      tft.setTextSize(2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("BLAD NASA", tft.width() / 2, tft.height() / 2 - 20);
      tft.setTextSize(1);
      tft.drawString("Sprawdz polaczenie", tft.width() / 2, tft.height() / 2 + 10);
      return;
    }
    
    // Próba pobrania i wyświetlenia
    if (downloadAndDisplayImage(tft, currentImage.imageNumber)) {
      Serial.println("✅ NASA obraz wyświetlony pomyślnie!");
      lastImageChange = millis();
      firstRun = false;
    } else {
      Serial.println("❌ Nie udało się wyświetlić NASA obrazka - próbuję inny...");
      
      // RETRY: Spróbuj 2 razy z różnymi obrazkami
      bool retrySuccess = false;
      for (int retry = 0; retry < 2 && !retrySuccess; retry++) {
        int retryImage = random(0, min(50, num_nasa_images)); // Pierwsze 50 są zazwyczaj najlżejsze/najpewniejsze
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
        
        if (loadFallbackImageFromLittleFS()) {
          Serial.println("✅ Fallback image załadowany!");
          firstRun = false;
          lastImageChange = millis();
          return;
        }
        
        // Krytyczny błąd (brak neta i brak fallbacka)
        tft.fillScreen(COLOR_BACKGROUND);
        tft.setTextColor(TFT_RED, COLOR_BACKGROUND);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("CRITICAL ERROR", tft.width() / 2, tft.height() / 2 - 30);
        tft.setTextSize(1);
        tft.drawString("NASA Failed", tft.width() / 2, tft.height() / 2 - 10);
        tft.drawString("Fallback Failed", tft.width() / 2, tft.height() / 2 + 10);
      }
    }
  }
}

// ================================================================
// FUNKCJA POBIERANIA I DEKODOWANIA (Engine)
// ================================================================
bool downloadAndDisplayImage(TFT_eSPI& tft, int imageIndex) {
  if (imageIndex >= num_nasa_images || imageIndex < 0) return false;
  
  NASAImage selectedImage = nasa_ultimate_collection[imageIndex];
  
  // Pokaż loading screen z animowanym spinnerem
  tft.fillScreen(TFT_BLACK);
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;
  tft.drawCircle(centerX, centerY, 20, TFT_CYAN);
  tft.drawCircle(centerX, centerY, 15, TFT_BLUE);
  tft.drawLine(centerX, centerY, centerX + 15, centerY - 10, TFT_WHITE);
  
  HTTPClient http;
  
  // Konfiguracja Timeoutów
  http.setTimeout(10000); 
  http.setConnectTimeout(5000);

  // Test Mode Override
  if (TEST_ONE_IMG == 1) {
     Serial.println("⚠️ TEST MODE: Podmieniam URL na testowy");
     selectedImage.url = "https://roccoss39.github.io/nasa.github.io-/nasa-images/NGC_3628_The_Hamburger_Galaxy.jpg";
  }

  if (DEBUG_IMAGES) Serial.printf("🌐 Connecting to: %s\n", selectedImage.url);
  http.begin(selectedImage.url);
  
  int httpCode = http.GET();
  
  if (httpCode != HTTP_CODE_OK) {
    Serial.println("❌ HTTP Error: " + String(httpCode));
    http.end();
    return false;
  }
  
  int contentLength = http.getSize();
  Serial.println("File size: " + String(contentLength) + " bytes");
  
  // --- ZMIANA: Zwiększony limit do 55KB ---
  if (contentLength > 55000) {
    Serial.println("⚠️ Obrazek zbyt duży (>55KB) dla bezpiecznego malloc! Pomijam.");
    http.end();
    return false; 
  }
  
  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("❌ No stream available");
    http.end();
    return false;
  }
  
  // Pobierz nagłówek (pierwsze 20 bajtów)
  uint8_t testBuffer[20];
  size_t testRead = stream->readBytes(testBuffer, min(20, contentLength));
  
  // Walidacja JPEG (Magic Bytes FF D8)
  bool looksLikeJPEG = (testRead >= 2 && testBuffer[0] == 0xFF && testBuffer[1] == 0xD8);
  if (!looksLikeJPEG) {
    Serial.println("❌ ERROR: To nie jest plik JPEG!");
    http.end();
    return false;
  }
  
  // Alokacja pamięci
  uint8_t* buffer = (uint8_t*)malloc(contentLength);
  if (!buffer) {
    Serial.println("❌ Memory allocation failed");
    http.end();
    return false;
  }
  
  // --- BLOKADA WiFi NA CZAS POBIERANIA ---
  extern bool isImageDownloadInProgress; 
  isImageDownloadInProgress = true;
  
  // Skopiuj nagłówek do bufora
  memcpy(buffer, testBuffer, testRead);
  
  // Pobierz resztę danych
  size_t remainingBytes = contentLength - testRead;
  size_t bytesRead = testRead;
  
  if (remainingBytes > 0) {
    unsigned long readStartTime = millis();
    size_t additionalRead = 0;
    size_t chunkSize = min(remainingBytes, (size_t)1024); // 1KB chunks
    
    while (additionalRead < remainingBytes) {
      if (millis() - readStartTime > 8000) break; // Timeout 8s
      
      if (stream->available() == 0) {
        delay(50);
        if (stream->available() == 0) {
           // Jeśli nadal brak danych po pauzie, możliwe zerwanie, ale spróbujmy pętli dalej
           if (millis() - readStartTime > 8000) break; 
           continue; 
        }
      }
      
      size_t toRead = min(chunkSize, remainingBytes - additionalRead);
      size_t chunkRead = stream->readBytes(buffer + testRead + additionalRead, toRead);
      additionalRead += chunkRead;
      
      if (chunkRead == 0) break;
    }
    bytesRead += additionalRead;
  }
  
  // --- ODBLOKOWANIE WiFi ---
  isImageDownloadInProgress = false;
  
  // --- RETRY MECHANISM (Jeśli pobranie niekompletne) ---
  if (bytesRead != contentLength) {
    Serial.printf("❌ INCOMPLETE TRANSFER! Missing %d bytes. Retrying...\n", contentLength - bytesRead);
    free(buffer);
    http.end();
    delay(500);
    
    // Próba #2
    http.setTimeout(15000); 
    http.begin(selectedImage.url);
    if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }
    
    int retryLen = http.getSize();
    WiFiClient* retryStream = http.getStreamPtr();
    buffer = (uint8_t*)malloc(retryLen);
    if (!buffer) { http.end(); return false; }
    
    isImageDownloadInProgress = true;
    size_t retryRead = 0;
    unsigned long retryStart = millis();
    
    while(retryRead < retryLen) {
        if(millis() - retryStart > 12000) break;
        if(retryStream->available()) {
            retryRead += retryStream->readBytes(buffer + retryRead, min((size_t)1024, (size_t)(retryLen - retryRead)));
        }
    }
    isImageDownloadInProgress = false;
    
    if (retryRead != retryLen) {
       Serial.println("❌ RETRY failed too.");
       free(buffer);
       http.end();
       return false;
    }
    contentLength = retryLen; // Sukces w retry
  }
  
  Serial.println("✅ Transfer complete. Decoding...");
  
  // Wyświetlanie
  tft.fillScreen(TFT_BLACK);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true); // Naprawia kolory na ESP32
  TJpgDec.setCallback(tft_output_nasa);
  
  int result = TJpgDec.drawJpg(0, 0, buffer, contentLength);
  
  if (result == 0) {
    // Sukces - Rysuj tytuł
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    
    String titleStr = String(selectedImage.title);
    if (titleStr.length() > 45) titleStr = titleStr.substring(0, 42) + "...";
    tft.drawString(titleStr, tft.width() / 2, 225);
    
    free(buffer);
    http.end();
    return true;
  } else {
    Serial.println("❌ JPEG Decode Error: " + String(result));
    free(buffer);
    http.end();
    return false;
  }
}