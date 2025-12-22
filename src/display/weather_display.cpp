#include "display/weather_display.h"
#include "managers/WeatherCache.h"

// Singleton instance WeatherCache  
static WeatherCache weatherCache;

WeatherCache& getWeatherCache() {
  return weatherCache;
}
#include "display/weather_icons.h"
#include "config/display_config.h"
#include "weather/forecast_data.h"


bool hasWeatherChanged() {
  return getWeatherCache().hasChanged(weather);
}

void updateWeatherCache() {
  getWeatherCache().updateCache(weather);
}


