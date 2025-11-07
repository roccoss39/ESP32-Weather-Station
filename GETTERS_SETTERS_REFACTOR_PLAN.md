# 🎯 Getters/Setters Refactoring Plan

## 🚀 **OOP UPGRADE - Zamiana extern na gettery/settery**

### **PROBLEM Z EXTERN:**
```cpp
// ❌ C-style - global mess
extern float weatherCachePrev_temperature;
extern String weatherCachePrev_description;
extern DisplayState currentDisplayState;
extern WeatherData weather;
```

### **✅ ROZWIĄZANIE - C++ Classes:**

## **1. 🌤️ WeatherCache Class**
```cpp
class WeatherCache {
private:
    float prevTemperature = -999.0f;
    float prevFeelsLike = -999.0f;
    float prevHumidity = -999.0f;
    float prevWindSpeed = -999.0f;
    float prevPressure = -999.0f;
    String prevDescription = "";
    String prevIcon = "";

public:
    // Getters
    float getPrevTemperature() const { return prevTemperature; }
    float getPrevFeelsLike() const { return prevFeelsLike; }
    String getPrevDescription() const { return prevDescription; }
    
    // Setters
    void setPrevTemperature(float temp) { prevTemperature = temp; }
    void setPrevFeelsLike(float feels) { prevFeelsLike = feels; }
    void setPrevDescription(const String& desc) { prevDescription = desc; }
    
    // Utility
    void resetCache() {
        prevTemperature = -999.0f;
        prevFeelsLike = -999.0f;
        prevHumidity = -999.0f;
        prevWindSpeed = -999.0f;
        prevPressure = -999.0f;
        prevDescription = "";
        prevIcon = "";
    }
    
    bool hasChanged(const WeatherData& current) const {
        return (prevTemperature != current.temperature ||
                prevDescription != current.description ||
                prevIcon != current.icon);
    }
};
```

## **2. ⏰ TimeDisplayCache Class**
```cpp
class TimeDisplayCache {
private:
    char prevTimeStr[9] = "";
    char prevDateStr[11] = "";
    String prevDayStr = "";
    int prevWifiStatus = -1;

public:
    // Getters
    const char* getPrevTimeStr() const { return prevTimeStr; }
    const char* getPrevDateStr() const { return prevDateStr; }
    String getPrevDayStr() const { return prevDayStr; }
    int getPrevWifiStatus() const { return prevWifiStatus; }
    
    // Setters
    void setPrevTimeStr(const char* timeStr) { 
        strncpy(prevTimeStr, timeStr, sizeof(prevTimeStr)-1); 
    }
    void setPrevDateStr(const char* dateStr) { 
        strncpy(prevDateStr, dateStr, sizeof(prevDateStr)-1); 
    }
    void setPrevDayStr(const String& dayStr) { prevDayStr = dayStr; }
    void setPrevWifiStatus(int status) { prevWifiStatus = status; }
    
    // Utility
    void resetCache() {
        strcpy(prevTimeStr, "");
        strcpy(prevDateStr, "");
        prevDayStr = "";
        prevWifiStatus = -1;
    }
};
```

## **3. 🔍 MotionSensorManager Class**
```cpp
class MotionSensorManager {
private:
    volatile bool motionDetected = false;
    DisplayState currentState = DISPLAY_SLEEPING;
    unsigned long lastMotionTime = 0;
    unsigned long lastDisplayUpdate = 0;

public:
    // Getters
    bool isMotionDetected() const { return motionDetected; }
    DisplayState getDisplayState() const { return currentState; }
    unsigned long getLastMotionTime() const { return lastMotionTime; }
    unsigned long getLastDisplayUpdate() const { return lastDisplayUpdate; }
    
    // Setters
    void setMotionDetected(bool detected) { motionDetected = detected; }
    void setDisplayState(DisplayState state) { currentState = state; }
    void updateMotionTime() { lastMotionTime = millis(); }
    void updateDisplayTime() { lastDisplayUpdate = millis(); }
    
    // Business Logic
    bool isMotionTimeout() const {
        return (millis() - lastMotionTime) > MOTION_TIMEOUT;
    }
    
    void handleMotionInterrupt() {
        motionDetected = true;
        lastMotionTime = millis();
        if (currentState == DISPLAY_SLEEPING) {
            currentState = DISPLAY_ACTIVE;
        }
    }
};
```

## **4. 📱 ScreenManager Class**
```cpp
class ScreenManager {
private:
    ScreenType currentScreen = SCREEN_CURRENT_WEATHER;
    unsigned long lastScreenSwitch = 0;
    static const unsigned long SCREEN_SWITCH_INTERVAL = 10000;

public:
    // Getters
    ScreenType getCurrentScreen() const { return currentScreen; }
    unsigned long getLastSwitch() const { return lastScreenSwitch; }
    
    // Setters
    void setCurrentScreen(ScreenType screen) { currentScreen = screen; }
    
    // Business Logic
    bool shouldSwitchScreen() const {
        return (millis() - lastScreenSwitch) >= SCREEN_SWITCH_INTERVAL;
    }
    
    void switchToNext() {
        lastScreenSwitch = millis();
        
        switch(currentScreen) {
            case SCREEN_CURRENT_WEATHER:
                currentScreen = SCREEN_FORECAST;
                break;
            case SCREEN_FORECAST:
                currentScreen = SCREEN_IMAGE;
                break;
            case SCREEN_IMAGE:
                currentScreen = SCREEN_CURRENT_WEATHER;
                break;
        }
    }
};
```

## **5. 🌍 WeatherDataManager Class**
```cpp
class WeatherDataManager {
private:
    WeatherData currentWeather;
    ForecastData currentForecast;
    unsigned long lastWeatherUpdate = 0;
    unsigned long lastForecastUpdate = 0;

public:
    // Getters
    const WeatherData& getWeather() const { return currentWeather; }
    const ForecastData& getForecast() const { return currentForecast; }
    unsigned long getLastWeatherUpdate() const { return lastWeatherUpdate; }
    unsigned long getLastForecastUpdate() const { return lastForecastUpdate; }
    
    // Setters
    void setWeather(const WeatherData& weather) { 
        currentWeather = weather; 
        lastWeatherUpdate = millis();
    }
    void setForecast(const ForecastData& forecast) { 
        currentForecast = forecast; 
        lastForecastUpdate = millis();
    }
    
    // Business Logic
    bool needsWeatherUpdate() const {
        return (millis() - lastWeatherUpdate) > WEATHER_UPDATE_INTERVAL;
    }
    
    bool needsForecastUpdate() const {
        return (millis() - lastForecastUpdate) > WEATHER_UPDATE_INTERVAL;
    }
};
```

## **🔄 REFACTORING STEPS:**

### **Phase 1: Create Classes**
1. Create `include/managers/` folder
2. Implement `WeatherCache.h/.cpp`
3. Implement `MotionSensorManager.h/.cpp`
4. Implement `ScreenManager.h/.cpp`

### **Phase 2: Replace extern calls**
1. Replace extern weather cache with WeatherCache instance
2. Replace extern motion vars with MotionSensorManager
3. Replace extern screen vars with ScreenManager

### **Phase 3: Update main.cpp**
1. Create manager instances
2. Update function calls to use getters/setters
3. Remove all extern declarations

## **📊 KORZYŚCI:**

### **✅ Enkapsulacja:**
- Private data - brak globalnego dostępu
- Controlled access przez getters/setters
- Business logic w klasach

### **✅ Type Safety:**
- Compile-time checking
- Brak extern conflicts
- Lepsze error messages

### **✅ Maintainability:**
- Clear interfaces
- Easy testing
- Better organization

### **✅ Performance:**
- Inline functions
- No overhead vs extern
- Better optimization potential

## **🎯 IMPLEMENTATION ORDER:**

**Priority 1: WeatherCache** (największy impact)
**Priority 2: MotionSensorManager** (business logic)  
**Priority 3: ScreenManager** (clean interfaces)
**Priority 4: WeatherDataManager** (global state)