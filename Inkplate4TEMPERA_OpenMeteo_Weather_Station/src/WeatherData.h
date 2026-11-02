#ifndef WEATHERDATA_H
#define WEATHERDATA_H

#include <Arduino.h>

// Plain data only (no String members) so a copy can live in RTC memory across deep sleep.
struct WeatherData {
    bool valid;
    bool metric;
    float currentTemp;
    float feelsLike;
    float windSpeed;
    float precipitation;
    float uvIndex;
    bool isDay;
    int weatherCode;
    char weatherDescription[32];
    char sunrise[8];
    char sunset[8];
    char updatedTime[8]; // local time of the last successful fetch, "HH:MM"

    float dailyMinTemp[7];
    float dailyMaxTemp[7];
    int dailyWeatherCodes[7];
    char dailyNames[7][4];

    static const int kHourlySlots = 6;
    float hourlyTemps[kHourlySlots];
    float hourlyPrecip[kHourlySlots];
    char hourlyTimes[kHourlySlots][8];
    int hourlyWeatherCodes[kHourlySlots];
    bool hourlyAvailable[kHourlySlots];
};

inline bool isRainCode(int code)
{
    return (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
}

inline bool isSnowCode(int code)
{
    return (code >= 71 && code <= 77) || code == 85 || code == 86;
}

inline bool isStormCode(int code)
{
    return code >= 95 && code <= 99;
}

#endif
