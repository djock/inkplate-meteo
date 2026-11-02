#include "Network.h"
#include <WiFi.h>          
#include <HTTPClient.h>     
#include "WeatherData.h"     

#include <ArduinoJson.h>     

#include <Inkplate.h>


// Function to get weather description based on the weather code
String Network::getWeatherDescription(int code) {
  switch (code) {
      case 0: return "Clear sky";
      case 1: case 2: case 3: return "Mainly clear, partly cloudy";
      case 45: case 48: return "Fog";
      case 51: case 53: case 55: return "Drizzle";
      case 56: case 57: return "Freezing Drizzle";
      case 61: case 63: case 65: return "Rain";
      case 66: case 67: return "Freezing Rain";
      case 71: case 73: case 75: return "Snowfall";
      case 77: return "Snow grains";
      case 80: case 81: case 82: return "Rain showers";
      case 85: case 86: return "Snow showers";
      case 95: return "Thunderstorm";
      case 96: case 99: return "Thunderstorm, hail";
      default: return "Unknown condition";
  }
}

String Network::extractDate(String dateTime) {
  int spaceIndex = dateTime.indexOf(' ');
  if (spaceIndex != -1) {
      return dateTime.substring(0, spaceIndex);  // e.g., "2025-04-08"
  }
  return "????-??-??";
}


String Network::extractTime(String dateTime) {
  int tIndex = dateTime.indexOf(' ');
  if (tIndex != -1 && tIndex + 5 < dateTime.length()) {
      return dateTime.substring(tIndex + 1, tIndex + 6);  // e.g., "06:11"
  }
  return "??:??";
}

String Network::getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      return "Time N/A";
  }

  char timeStr[20];
  snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d",
           timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday,
           timeinfo.tm_hour,
           timeinfo.tm_min);
  return String(timeStr);
}

int Network::getCurrentHour() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      return -1;  // Return -1 if time is not available (error)
  }

  return timeinfo.tm_hour;
}

String Network::extractSun(String dateTime) {
  int tIndex = dateTime.indexOf('T');
  if (tIndex != -1 && tIndex + 5 < dateTime.length()) {
      return dateTime.substring(tIndex + 1, tIndex + 6);  // e.g., "06:11"
  }
  return "??:??";
}

// Function to get the current day of the week and return the next 7 days dynamically
String Network::getDayName(int dayIndex) {
  // Array of days, starting from Sunday (index 0)
  String daysOfWeek[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  
  // Get the current time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
      return "Error";  // Return error if time can't be fetched
  }

  // Get the current day index (0 = Sunday, 1 = Monday, etc.)
  int currentDay = timeinfo.tm_wday;

  // Calculate the day of the week for the given index
  int dayOfWeekIndex = (currentDay + dayIndex) % 7;

  return daysOfWeek[dayOfWeekIndex];
}

// Copy a String into a fixed char buffer (always NUL terminated).
static void copyTo(char *dst, size_t size, const String &src)
{
    strlcpy(dst, src.c_str(), size);
}

// Function to fetch weather data from Open-Meteo API
void Network::fetchWeatherData(WeatherData* weatherData, UserInfo* userInfo, const float* latitude, const float* longitude) {
    String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(*latitude, 4) + "&longitude=" + String(*longitude, 4) +
                 "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset,wind_speed_10m_max,wind_direction_10m_dominant,precipitation_probability_max,weather_code,uv_index_max" +
                 "&hourly=temperature_2m,precipitation_probability,weather_code" +
                 "&current=temperature_2m,precipitation,wind_speed_10m,weather_code,apparent_temperature,relative_humidity_2m,is_day" +
                 "&timezone=auto&forecast_days=7";

    if (userInfo->useMetric) {
      userInfo->temperatureLabel = " C";
      userInfo->speedLabel = " km/h";
    }
    else {
      userInfo->temperatureLabel = " F";
      userInfo->speedLabel = " mph";
      url += "&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch";
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(15000);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("Weather request failed: %d\n", httpCode);
        userInfo->apiError = true;
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("Weather JSON error: %s\n", err.c_str());
        userInfo->apiError = true;
        return;
    }
    userInfo->apiError = false;

    userInfo->lastUpdated = getFormattedTime();
    userInfo->currentHour = getCurrentHour();
    userInfo->lastUpdatedDate = extractDate(userInfo->lastUpdated);
    userInfo->lastUpdatedTime = extractTime(userInfo->lastUpdated);

    weatherData->valid = true;
    weatherData->metric = userInfo->useMetric;
    weatherData->currentTemp = doc["current"]["temperature_2m"].as<float>();
    weatherData->feelsLike = doc["current"]["apparent_temperature"].as<float>();
    copyTo(weatherData->sunrise, sizeof(weatherData->sunrise), extractSun(doc["daily"]["sunrise"][0].as<String>()));
    copyTo(weatherData->sunset, sizeof(weatherData->sunset), extractSun(doc["daily"]["sunset"][0].as<String>()));
    weatherData->uvIndex = doc["daily"]["uv_index_max"][0].as<float>();
    weatherData->precipitation = doc["current"]["precipitation"].as<float>();
    weatherData->windSpeed = doc["current"]["wind_speed_10m"].as<float>();
    weatherData->isDay = doc["current"]["is_day"].as<bool>();
    weatherData->weatherCode = doc["current"]["weather_code"].as<int>();
    copyTo(weatherData->weatherDescription, sizeof(weatherData->weatherDescription), getWeatherDescription(weatherData->weatherCode));
    copyTo(weatherData->updatedTime, sizeof(weatherData->updatedTime), userInfo->lastUpdatedTime);

    for (int i = 0; i < 7; i++) {
        weatherData->dailyMinTemp[i] = doc["daily"]["temperature_2m_min"][i].as<float>();
        weatherData->dailyMaxTemp[i] = doc["daily"]["temperature_2m_max"][i].as<float>();
        weatherData->dailyWeatherCodes[i] = doc["daily"]["weather_code"][i].as<int>();
        copyTo(weatherData->dailyNames[i], sizeof(weatherData->dailyNames[i]), getDayName(i));
    }

    // Next hourly slots, starting at the current local hour
    int hourlyCount = doc["hourly"]["time"].size();
    for (int i = 0; i < WeatherData::kHourlySlots; i++) {
        int sourceIndex = userInfo->currentHour + i;
        bool hasData = (userInfo->currentHour >= 0) && (sourceIndex < hourlyCount);

        weatherData->hourlyAvailable[i] = hasData;
        if (hasData) {
            weatherData->hourlyTemps[i] = doc["hourly"]["temperature_2m"][sourceIndex].as<float>();
            weatherData->hourlyPrecip[i] = doc["hourly"]["precipitation_probability"][sourceIndex].as<float>();
            copyTo(weatherData->hourlyTimes[i], sizeof(weatherData->hourlyTimes[i]), extractSun(doc["hourly"]["time"][sourceIndex].as<String>()));
            weatherData->hourlyWeatherCodes[i] = doc["hourly"]["weather_code"][sourceIndex].as<int>();
        } else {
            weatherData->hourlyTemps[i] = 0.0f;
            weatherData->hourlyPrecip[i] = 0.0f;
            weatherData->hourlyTimes[i][0] = '\0';
            weatherData->hourlyWeatherCodes[i] = 0;
        }
    }
}
