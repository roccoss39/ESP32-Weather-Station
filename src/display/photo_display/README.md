# 🚀 ESP32 NASA Image Display Project

Automatyczny wyświetlacz 401 obrazków NASA na ESP32 z TFT ekranem. Projekt pobiera obrazy z GitHub Pages i wyświetla je z automatyczną rotacją co 10 sekund.

## 📱 Funkcjonalności

### ✨ Główne cechy:
- **401 obrazków NASA** z lat 2024-2025 (cały archiwum!)
- **Automatyczna rotacja** co 10 sekund
- **WiFi connection** z automatycznym reconnect
- **HTTP download** obrazków w czasie rzeczywistym
- **JPEG dekoding** i skalowanie do 320x240
- **Wyświetlanie tytułów** każdego obrazka
- **Licznik postępu** (1/401, 2/401, etc.)
- **Serial monitor** z informacjami o pobieraniu
- **Endless loop** - po ostatnim obrazku wraca do pierwszego

### 🖥️ Interfejs:
- **Splash screen**: "NASA Archive" podczas uruchamiania
- **WiFi status**: Informacje o połączeniu
- **Image title**: Nazwa obrazka na górze ekranu
- **Progress counter**: Aktualny/całkowity na dole
- **Serial output**: Szczegółowe logi pobierania

## 🔧 Wymagania sprzętowe

### ESP32 Board:
- **ESP32 DevKit** lub kompatybilny
- **Flash**: minimum 4MB (projekt używa 78.7%)
- **RAM**: minimum 320KB (projekt używa 15.6%)
- **WiFi**: 2.4GHz WPA2/WPA3

### TFT Display:
- **Model**: ILI9341 lub ST7789 (320x240)
- **Interfejs**: SPI
- **Testowane na**: JC2432S028 (Cheap Yellow Display)
- **Touch**: opcjonalne (projekt nie używa)

### Połączenie pinów (domyślne TFT_eSPI):
```
TFT_CS    = 15   // Chip Select
TFT_DC    = 2    // Data/Command
TFT_MOSI  = 13   // SPI Data
TFT_SCLK  = 14   // SPI Clock
TFT_RST   = 12   // Reset (opcjonalne)
TFT_BL    = 21   // Backlight (opcjonalne)
```

## 📂 Struktura projektu

```
📁 ESP32-NASA-Display/
├── 📄 README.md                    ← Ten plik
├── 📄 platformio.ini               ← Konfiguracja PlatformIO
├── 📄 wifi_config.h                ← Template WiFi credentials
├── 📄 esp32_nasa_ultimate.h        ← Array z 401 obrazkami NASA
├── 📄 .gitignore                   ← Git exclusions
└── 📁 src/
    ├── 📄 main.cpp                 ← Główny kod ESP32
    └── 📄 esp32_nasa_array.h       ← Backup array (9 obrazków)
```

## ⚡ Quick Start

### 1. Przygotowanie środowiska:
```bash
# PlatformIO (zalecane)
pip install platformio
pio lib install

# Lub Arduino IDE + biblioteki:
# - TFT_eSPI
# - ArduinoJson (opcjonalne)
```

### 2. Konfiguracja WiFi:
```cpp
// W src/main.cpp linie 7-8:
const char* WIFI_SSID = "TWOJA_SIEC_WIFI";     // ← ZMIEŃ!
const char* WIFI_PASSWORD = "TWOJE_HASLO";     // ← ZMIEŃ!
```

### 3. Konfiguracja TFT (jeśli inne piny):
```cpp
// W src/main.cpp lub User_Setup.h w TFT_eSPI:
#define TFT_CS    15
#define TFT_DC    2  
#define TFT_MOSI  13
#define TFT_SCLK  14
```

### 4. Upload na ESP32:
```bash
# PlatformIO:
pio run --target upload

# Arduino IDE:
# Compile & Upload (Ctrl+U)
```

### 5. Monitor Serial (opcjonalnie):
```bash
pio device monitor --baud 115200
```

## 🌐 Źródło obrazków

### GitHub Pages URL:
```
https://roccoss39.github.io/nasa.github.io-/nasa-images/
```

### Przykład URL obrazka:
```
https://roccoss39.github.io/nasa.github.io-/nasa-images/nasa_2024-01-01_NGC_1232_spiral_galaxy.jpg
```

### Struktura pliku `esp32_nasa_ultimate.h`:
```cpp
struct NASAImage {
  const char* url;      // Pełny URL obrazka
  const char* filename; // Nazwa pliku 
  const char* title;    // Tytuł do wyświetlenia
};

NASAImage nasa_ultimate_collection[] = {
  {"https://roccoss39.github.io/nasa.github.io-/nasa-images/nasa_2024-01-01_NGC_1232_spiral_galaxy.jpg", 
   "nasa_2024-01-01_NGC_1232_spiral_galaxy.jpg", 
   "2024-01-01 NGC 1232 A Grand Design Spiral Galaxy"},
  // ... 400 więcej obrazków
};

const int num_nasa_images = 401;
```

## 🔄 Działanie programu

### Sekwencja uruchomienia:
1. **Setup()**: Inicjalizacja TFT, WiFi, Serial
2. **Splash Screen**: "NASA Archive" przez 2 sekundy  
3. **WiFi Connect**: Automatyczne łączenie z siecią
4. **First Image**: Pobieranie i wyświetlanie pierwszego obrazka
5. **Loop**: Co 10 sekund nowy obrazek

### Loop główny:
```cpp
void loop() {
  static unsigned long lastImageChange = 0;
  
  if (millis() - lastImageChange >= 10000) {  // 10 sekund
    current_image_index = (current_image_index + 1) % 401;
    downloadAndDisplayImage(current_image_index);
    lastImageChange = millis();
  }
  
  delay(100);  // CPU relief
}
```

### Serial Output przykład:
```
=== ESP32 NASA SEQUENTIAL DISPLAY ===
📸 Total NASA images: 401
🔄 Advancing to image 1
=== Image 1/401 ===
URL: https://roccoss39.github.io/nasa.github.io-/...
Filename: nasa_2024-01-01_NGC_1232_spiral_galaxy.jpg
📱 Connecting to WiFi: TWOJA_SIEC
✅ Connected! IP: 192.168.1.100
🌐 Starting HTTP request...
📦 Content-Length: 45678 bytes
📥 Downloaded successfully!
🖼️ JPEG decoded: 320x240 pixels
✅ Image displayed successfully!
```

## 🛠️ Dostosowywanie

### Zmiana interwału wyświetlania:
```cpp
// W src/main.cpp linia 20:
const unsigned long image_change_interval = 10000;  // 10 sekund
// Zmień na np. 30000 dla 30 sekund
```

### Zmiana rozmiaru tekstu:
```cpp
// W funkcji displayImageTitle():
tft.setTextSize(2);  // Zmień na 1-4
```

### Dodanie własnych obrazków:
1. Umieść obrazki na swojej stronie/serwerze
2. Dodaj do `esp32_nasa_ultimate.h`:
```cpp
{"http://twoja-strona.com/obrazek.jpg", "nazwa.jpg", "Tytuł obrazka"},
```
3. Zwiększ `num_nasa_images`

### Zmiana koloru tła:
```cpp
// W src/main.cpp:
tft.fillScreen(TFT_BLACK);  // Zmień na TFT_BLUE, TFT_RED, etc.
```

## 📊 Statystyki kompilacji

```
RAM:   [====      ]  15.6% (używane 50,960 z 327,680 bajtów)
Flash: [========  ]  78.7% (używane 1,031,389 z 1,310,720 bajtów)
```

### Biblioteki używane:
- **TFT_eSPI**: Sterownik wyświetlacza TFT
- **WiFi**: Wbudowana biblioteka ESP32 WiFi
- **HTTPClient**: HTTP requests
- **TJpg_Decoder**: Dekodowanie JPEG

## 🔧 Rozwiązywanie problemów

### Problem: "WiFi connection failed"
```cpp
// Sprawdź credentials w main.cpp
const char* WIFI_SSID = "poprawna_nazwa";
const char* WIFI_PASSWORD = "poprawne_haslo";
```

### Problem: "HTTP request failed"
- Sprawdź czy GitHub Pages działa: [link](https://roccoss39.github.io/nasa.github.io-/)
- Sprawdź połączenie internetowe ESP32
- Zobacz Serial Monitor dla szczegółów błędu

### Problem: "JPEG decode failed"
- Obrazek może być uszkodzony
- Sprawdź URL w przeglądarce
- Zwiększ timeout w HTTPClient

### Problem: Wyświetlacz nie działa
```cpp
// Sprawdź piny w User_Setup.h w TFT_eSPI
#define TFT_CS    15  // Chip Select
#define TFT_DC    2   // Data Command
#define TFT_MOSI  13  // SPI Data
#define TFT_SCLK  14  // SPI Clock
```

### Problem: Brak pamięci
- ESP32 musi mieć minimum 4MB Flash
- Sprawdź `partition table` w platformio.ini
- Zmniejsz rozdzielczość obrazków

## 📝 Historia zmian

### v1.0 (2024-11-04):
- ✅ Implementacja podstawowego wyświetlania
- ✅ 401 obrazków NASA z GitHub Pages
- ✅ Automatyczna rotacja co 10 sekund
- ✅ WiFi auto-reconnect
- ✅ Serial monitoring
- ✅ JPEG dekoding i skalowanie

### Planowane ulepszenia:
- 🔄 Touch control (next/prev)
- 🔄 Web interface do zmiany ustawień
- 🔄 OTA updates
- 🔄 RTC dla wyświetlania daty/czasu
- 🔄 Sleep mode dla oszczędzania energii

## 👨‍💻 Autor

Projekt ESP32 NASA Image Display  
GitHub: [roccoss39](https://github.com/roccoss39)  
Obrazy NASA: [NASA APOD](https://apod.nasa.gov/)

## 📄 Licencja

MIT License - użyj jak chcesz, na własną odpowiedzialność.

## 🚀 Quick Commands

```bash
# Kompilacja
pio run

# Upload na ESP32  
pio run --target upload

# Monitor Serial
pio device monitor

# Clean build
pio run --target clean

# Lista urządzeń
pio device list
```

---
**🌌 Enjoy your NASA slideshow on ESP32! 🚀**