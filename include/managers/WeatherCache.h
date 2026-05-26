#ifndef WEATHER_CACHE_H
#define WEATHER_CACHE_H

#include <Arduino.h>
#include "weather/weather_data.h"

class WeatherCache {
private:
    // Previous values for comparison
    float prevTemperature = -999.0f;
    float prevFeelsLike = -999.0f;
    float prevHumidity = -999.0f;
    float prevWindSpeed = -999.0f;
    float prevPressure = -999.0f;
    String prevDescription = "";
    String prevIcon = "";

public:
    // --- GETTERS ---
    float getPrevTemperature() const { return prevTemperature; }
    float getPrevFeelsLike() const { return prevFeelsLike; }
    float getPrevHumidity() const { return prevHumidity; }
    float getPrevWindSpeed() const { return prevWindSpeed; }
    float getPrevPressure() const { return prevPressure; }
    String getPrevDescription() const { return prevDescription; }
    String getPrevIcon() const { return prevIcon; }
    
    // --- SETTERS ---
    void setPrevTemperature(float temp) { prevTemperature = temp; }
    void setPrevFeelsLike(float feels) { prevFeelsLike = feels; }
    void setPrevHumidity(float humidity) { prevHumidity = humidity; }
    void setPrevWindSpeed(float wind) { prevWindSpeed = wind; }
    void setPrevPressure(float pressure) { prevPressure = pressure; }
    void setPrevDescription(const String& desc) { prevDescription = desc; }
    void setPrevIcon(const String& icon) { prevIcon = icon; }
    
    // --- UTILITY METHODS ---
    
    /**
     * Resetuje cały cache - wymusza ponowne rysowanie
     * Użyj przy przełączaniu ekranów
     */
    void resetCache() {
        prevTemperature = -999.0f;
        prevFeelsLike = -999.0f;
        prevHumidity = -999.0f;
        prevWindSpeed = -999.0f;
        prevPressure = -999.0f;
        prevDescription = "";
        prevIcon = "";
    }
    
    /**
     * Sprawdza czy dane pogodowe się zmieniły
     * @param current Aktualne dane pogodowe
     * @return true jeśli coś się zmieniło
     */
    bool hasChanged(const WeatherData& current) const {
        return (prevTemperature != current.temperature ||
                prevFeelsLike != current.feelsLike ||
                prevHumidity != current.humidity ||
                prevWindSpeed != current.windSpeed ||
                prevPressure != current.pressure ||
                prevDescription != current.description ||
                prevIcon != current.icon);
    }
    
    /**
     * Aktualizuje cache nowymi wartościami
     * @param current Aktualne dane pogodowe
     */
    void updateCache(const WeatherData& current) {
        prevTemperature = current.temperature;
        prevFeelsLike = current.feelsLike;
        prevHumidity = current.humidity;
        prevWindSpeed = current.windSpeed;
        prevPressure = current.pressure;
        prevDescription = current.description;
        prevIcon = current.icon;
    }
    
    /**
     * Smart update - aktualizuje cache tylko jeśli się zmienił
     * @param current Aktualne dane pogodowe
     * @return true jeśli cache został zaktualizowany
     */
    bool smartUpdate(const WeatherData& current) {
        if (hasChanged(current)) {
            updateCache(current);
            return true;
        }
        return false;
    }
    
    // --- DEBUG ---
    void printDebugInfo() const {
        Serial.println("=== WeatherCache Debug ===");
        Serial.println("Temp: " + String(prevTemperature) + "°C");
        Serial.println("Feels: " + String(prevFeelsLike) + "°C");
        Serial.println("Humidity: " + String(prevHumidity) + "%");
        Serial.println("Wind: " + String(prevWindSpeed) + " m/s");
        Serial.println("Pressure: " + String(prevPressure) + " hPa");
        Serial.println("Description: " + prevDescription);
        Serial.println("Icon: " + prevIcon);
    }
};

#endif