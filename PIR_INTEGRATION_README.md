# 🔥 PIR Motion Sensor Integration - ESP32 Weather Station

## 📋 Podsumowanie integracji

Czujnik ruchu PIR MOD-01655 został w pełni zintegrowany z ESP32 Weather Station, dodając funkcjonalność **deep sleep** z automatycznym budzeniem na ruch.

## 🚀 Jak działa system

### 1. **Cold Start (pierwszy restart)**
```
ESP32 → Inicjalizacja → PIR Setup → Display ACTIVE (30s demo) → Deep Sleep
```

### 2. **Motion Detection Wake Up**
```
PIR wykrywa ruch → ESP32 budzi się → Display ACTIVE → Stacja pogodowa działa 30s → Sleep
```

### 3. **Cykl pracy**
- **ACTIVE**: 30 sekund wyświetlania pogody/prognozy/zdjęć NASA
- **SLEEP**: Deep sleep z czekaniem na PIR interrupt
- **WAKE UP**: Natychmiastowe budzenie na ruch

## ⚡ Oszczędzanie energii

### Deep Sleep Power Consumption:
- **ACTIVE**: ~200-300mA (WiFi + TFT + CPU)
- **DEEP SLEEP**: ~10-50μA (tylko PIR monitoring)
- **Oszczędność**: 99.9% redukcja zużycia energii!

### Czas pracy na baterii:
- **Bez deep sleep**: ~6-10 godzin
- **Z deep sleep**: ~2-6 miesięcy (zależnie od aktywności PIR)

## 🔧 Pliki zmodyfikowane

### 1. **src/main.cpp**
```cpp
// Dodano:
#include "sensors/motion_sensor.h"
#include <esp_sleep.h>

// Setup():
- Detekcja wake up reason
- Inicjalizacja PIR
- Auto-aktywacja na PIR wake up

// Loop():
- updateDisplayPowerState(tft) na początku
- Blokada działania gdy DISPLAY_SLEEPING
```

### 2. **src/display/screen_manager.cpp**
```cpp
// Dodano:
#include "sensors/motion_sensor.h"

// updateScreenManager():
- Blokada przełączania ekranów gdy display śpi
```

### 3. **src/sensors/motion_sensor.cpp**
```cpp
// Dodano:
#include <esp_sleep.h>

// initMotionSensor():
- Detekcja cold start vs PIR wake up
- Auto-ustawianie stanu display

// sleepDisplay():
- esp_sleep_enable_ext0_wakeup() 
- esp_deep_sleep_start()
- Prawdziwy deep sleep ESP32
```

### 4. **include/sensors/motion_sensor.h**
```cpp
// Dodano:
#include <TFT_eSPI.h>
```

### 5. **platformio.ini**
```ini
// Dodano:
-I include/sensors/
```

## 📱 Interfejs użytkownika

### Wake Up Message:
```
WAKE UP!
Motion detected
Starting weather station...
```

### Sleep Message:
```
SLEEP MODE
Waiting for motion...
PIR MOD-01655 active
Deep sleep in 3s...
```

### Serial Monitor:
```
=== ESP32 Weather Station ===
🔥 WAKE UP: PIR Motion Detected!
=== INICJALIZACJA PIR MOD-01655 ===
🔥 PIR WAKE UP - Display AKTYWNY
✅ PIR Sensor na GPIO 27 gotowy!
🕐 Timeout: 30 sekund
```

## ⚙️ Konfiguracja PIR

### Hardware:
```cpp
#define PIR_PIN 27                    // GPIO pin dla MOD-01655
#define MOTION_TIMEOUT 30000          // 30 sekund timeout
#define DEBOUNCE_TIME 500            // 500ms debounce
```

### Stany display:
```cpp
enum DisplayState {
  DISPLAY_SLEEPING = 0,   // Deep sleep, czeka na PIR
  DISPLAY_ACTIVE = 1,     // Aktywny, pokazuje stację pogodową  
  DISPLAY_TIMEOUT = 2     // Przejście do sleep
};
```

## 🔌 Połączenie PIR MOD-01655

```
PIR MOD-01655    ESP32
VCC      →       3.3V
GND      →       GND  
OUT      →       GPIO 27
```

### Uwagi:
- **VCC**: 3.3V (nie 5V!)
- **OUT**: Sygnał HIGH gdy ruch wykryty
- **Zasięg**: 3-7 metrów
- **Kąt**: 120 stopni

## 🧪 Testowanie

### Serial Commands (gdy aktywny):
```
f / F  - Wymusza aktualizację prognozy
w / W  - Wymusza aktualizację pogody
```

### Wake up test:
1. Poczekaj na deep sleep (30s bez ruchu)
2. Pomaż ręką przed PIR
3. ESP32 powinien się obudzić w <1 sekundę

### Power test:
```cpp
// Monitor zużycia prądu:
// ACTIVE: ~200-300mA
// SLEEP:  ~10-50μA
```

## 🚨 Rozwiązywanie problemów

### Problem: ESP32 nie budzi się na PIR
```cpp
// Sprawdź:
1. Połączenie GPIO 27
2. Zasilanie PIR 3.3V (nie 5V!)
3. Serial output: "esp_sleep_enable_ext0_wakeup"
```

### Problem: Za częste budzenie
```cpp
// Zwiększ DEBOUNCE_TIME:
#define DEBOUNCE_TIME 1000  // 1 sekunda
```

### Problem: Za długi timeout
```cpp
// Zmniejsz MOTION_TIMEOUT:
#define MOTION_TIMEOUT 15000  // 15 sekund
```

### Problem: PIR nie reaguje
```cpp
// Sprawdź wrażliwość PIR (potencjometry na module):
- Sx: Sensitivity (czułość)
- Tx: Time delay (czas aktywności)
```

## 🎯 Features

### ✅ Zaimplementowane:
- [x] PIR motion detection
- [x] ESP32 deep sleep
- [x] Auto wake up na ruch
- [x] 30s timeout bez ruchu
- [x] Rotacja ekranów pogoda/prognoza/NASA
- [x] Serial monitoring
- [x] Debounce protection
- [x] Cold start detection

### 🔄 Możliwe ulepszenia:
- [ ] Konfigurowalne timeout przez WiFi
- [ ] RTC wake up (backup timer)
- [ ] Brightness control na podstawie PIR
- [ ] Multiple PIR sensors
- [ ] Motion activity logging
- [ ] Battery voltage monitoring

## 💡 Przykład użycia

```cpp
// Podstawowe użycie - wszystko automatyczne!

void setup() {
    initMotionSensor();  // PIR ready
    // Display automatycznie ACTIVE na wake up
}

void loop() {
    updateDisplayPowerState(tft);  // Kontrola PIR
    
    if (getDisplayState() == DISPLAY_SLEEPING) {
        return; // ESP32 wejdzie w deep sleep
    }
    
    // Normalne działanie stacji pogodowej...
}
```

---

**🔥 Gotowe! PIR + Deep Sleep w pełni zintegrowany z ESP32 Weather Station! 🚀**
