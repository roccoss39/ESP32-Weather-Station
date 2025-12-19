# 🧪 NASA Images Availability Tester

Skrypty do testowania dostępności wszystkich 1359 obrazków NASA z `esp32_nasa_ultimate.h`.

## 📋 Dostępne Skrypty

### 1. **Python Version** (`test_nasa_images.py`)
- **Zalecany** - bardziej funkcjonalny
- Kolorowy output, szczegółowe statystyki
- Automatyczne zapisywanie błędnych URL-i
- Progress tracking z ETA

### 2. **Bash Version** (`test_nasa_images.sh`)  
- Alternatywa gdy brak Python
- Podstawowe testowanie z curl
- Prostsze, ale skuteczne

## 🚀 Użycie

### Python (Zalecane):
```bash
cd src/display/photo_display
python3 test_nasa_images.py
```

### Bash:
```bash
cd src/display/photo_display
./test_nasa_images.sh
```

## ⚙️ Konfiguracja

### Parametry w skryptach:
- **DELAY**: 1s (opóźnienie między requestami)
- **TIMEOUT**: 10s (timeout dla każdego obrazka) 
- **MAX_RETRIES**: 3 (próby dla nieudanych)
- **BASE_URL**: `https://roccoss39.github.io/nasa.github.io-/nasa-images/`

### Respectful Testing:
- ✅ 1 sekunda opóźnienia między requestami
- ✅ HEAD requests (szybsze niż GET)
- ✅ Proper timeout handling
- ✅ GitHub-friendly approach

## 📊 Output Przykładowy

```
🚀 NASA Images Availability Tester
===================================
Base URL: https://roccoss39.github.io/nasa.github.io-/nasa-images/
Delay between requests: 1.0s

📖 Reading esp32_nasa_ultimate.h...
✅ Found 1359 NASA image filenames

🧪 Starting test of 1359 images...
⏱️ Estimated time: 22.7 minutes

🔍 [   1/1359] 10_Days_of_Venus_and_Jupiter.jpg        ✅ OK (23KB)
🔍 [   2/1359] 2023_CX1_Meteor_Flash.jpg               ✅ OK (45KB)
🔍 [   3/1359] 21st_Century_Wet_Collodion_Moon.jpg     ❌ NOT FOUND (404)
...

📊 Progress: 100/1359 (7.4%) | Success: 98 | Failed: 2 | ETA: 21.2m
...

🏁 TEST COMPLETED
=================
Total images tested: 1359
Successful: 1340
Failed: 19
Success rate: 98.60%
Total time: 23.45 minutes
```

## 📁 Pliki Wyjściowe

### `failed_images.txt` 
Lista obrazków, które nie są dostępne:
```
Failed NASA Images:
==================

broken_image1.jpg - 404 Not Found
timeout_image2.jpg - Timeout
server_error3.jpg - HTTP 500
```

## 🛠️ Rozwiązywanie Problemów

### Błędy 404:
- Obrazek usunięty z GitHub
- Błąd w nazwie pliku
- **Akcja**: Usuń z `esp32_nasa_ultimate.h` lub znajdź replacement

### Timeouts:
- Problemy sieciowe
- Przeciążenie GitHub
- **Akcja**: Ponów test lub zwiększ TIMEOUT

### Rate Limiting:
- Za szybkie requesty  
- **Akcja**: Zwiększ DELAY

## 🎯 Typowe Wyniki

### Dobry Rezultat:
- **Success rate**: >95%
- **Failed**: <50 obrazków
- **Powód**: Pojedyncze usunięte/przeniesione pliki

### Problematyczny Rezultat:
- **Success rate**: <90%
- **Failed**: >100 obrazków  
- **Powód**: Systematyczny problem (URL change, repository move)

## 🔧 Maintenance

### Po teście:
1. **Sprawdź `failed_images.txt`**
2. **Usuń broken images z `esp32_nasa_ultimate.h`**
3. **Lub znajdź replacement URLs**
4. **Przetestuj ponownie subset**

### Aktualizacja collection:
```bash
# Test tylko subset (pierwsze 100)
head -100 esp32_nasa_ultimate.h > temp_subset.h
python3 test_nasa_images.py  # na temp_subset.h
```

## ⏰ Czas Wykonania

- **1359 obrazków × 1s delay = ~23 minuty**
- **Można przyspieszyć**: zmniejsz DELAY (ostrożnie!)
- **Parallel testing**: możliwe, ale nie zalecane (GitHub limits)

---

**🔥 Pro Tip**: Uruchom test w tle z logowaniem:
```bash
python3 test_nasa_images.py 2>&1 | tee test_results.log &
```