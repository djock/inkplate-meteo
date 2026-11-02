#include "Gui.h"
#include "WeatherData.h"
#include <Arduino.h>
#include <Inkplate.h>

// all the weather icons
#include "binary_Icons/icon_s_clear_sky.h"
#include "binary_Icons/icon_s_fog.h"
#include "binary_Icons/icon_s_gray.h"
#include "binary_Icons/icon_s_moon.h"
#include "binary_Icons/icon_s_partly_cloudy.h"
#include "binary_Icons/icon_s_rain.h"
#include "binary_Icons/icon_s_snow.h"
#include "binary_Icons/icon_s_storm.h"

// font
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSansBold12pt7b.h"
#include "fonts/FreeSansBold18pt7b.h"
#include "fonts/FreeSansBold24pt7b.h"
#include "fonts/FreeSansBold9pt7b.h"
#include "fonts/FreeSansBold48pt7b.h"

namespace
{
    // Drawing colors: always dark ink on white paper.
    uint16_t gInk = BLACK;
    uint16_t gPaper = WHITE;

    const int kScreenWidth = 600;
    const int kMargin = 24;

    void drawBitmap(Inkplate &d, int x, int y, const uint8_t *bitmap, int w, int h)
    {
        int bytesPerRow = (w + 7) / 8;
        for (int row = 0; row < h; row++)
        {
            for (int col = 0; col < w; col++)
            {
                if (pgm_read_byte(&bitmap[row * bytesPerRow + col / 8]) & (0x80 >> (col & 7)))
                {
                    d.drawPixel(x + col, y + row, gInk);
                }
            }
        }
    }

    // heavy = faux-bold (drawn twice, 1px apart) for the main numbers.
    void printText(Inkplate &d, int x, int y, const String &text, bool heavy)
    {
        d.setTextColor(gInk);
        d.setCursor(x, y);
        d.print(text);
        if (heavy)
        {
            d.setCursor(x + 1, y);
            d.print(text);
        }
    }

    void printHeavy(Inkplate &d, int x, int y, const String &text)
    {
        printText(d, x, y, text, true);
    }

    int textWidth(Inkplate &d, const String &text)
    {
        int16_t x1, y1;
        uint16_t w, h;
        d.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
        return w;
    }

    void centered(Inkplate &d, const String &text, int columnX, int columnW, int baselineY, bool heavy = true)
    {
        int x = columnX + (columnW - textWidth(d, text)) / 2;
        x = x < columnX ? columnX : x;
        printText(d, x, baselineY, text, heavy);
    }

    void rightAligned(Inkplate &d, const String &text, int rightX, int baselineY)
    {
        printHeavy(d, rightX - textWidth(d, text), baselineY, text);
    }

    String tempText(float value)
    {
        int v = (int)roundf(value);
        return String(v);
    }

    // Temperature centered in a column with a degree ring after the digits.
    void centeredTemp(Inkplate &d, float value, int columnX, int columnW, int baselineY, int ringR, int ringDrop)
    {
        String text = tempText(value);
        int total = textWidth(d, text) + 2 * ringR + 4;
        int x = columnX + (columnW - total) / 2;
        printHeavy(d, x, baselineY, text);
        int ringX = x + textWidth(d, text) + ringR + 4;
        d.drawCircle(ringX, baselineY - ringDrop, ringR, gInk);
        if (ringR > 4)
        {
            d.drawCircle(ringX, baselineY - ringDrop, ringR - 1, gInk);
        }
    }

    // Highest precipitation probability among the upcoming hours whose weather matches the class.
    int chanceFor(const WeatherData &w, bool (*matches)(int))
    {
        int best = 0;
        for (int i = 0; i < WeatherData::kHourlySlots; i++)
        {
            if (w.hourlyAvailable[i] && matches(w.hourlyWeatherCodes[i]))
            {
                int p = (int)w.hourlyPrecip[i];
                if (p > best)
                {
                    best = p;
                }
            }
        }
        return best;
    }

    bool matchesRain(int code) { return isRainCode(code); }
    bool matchesSnow(int code) { return isSnowCode(code); }
    bool matchesStorm(int code) { return isStormCode(code); }

    String dateLine()
    {
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 100))
        {
            return "";
        }
        char buffer[40];
        strftime(buffer, sizeof(buffer), "%A %d %b", &timeinfo);
        return String(buffer);
    }
}

Gui::Gui(Inkplate &inkplate) : inkplate(inkplate)
{
}

void Gui::fullRefresh()
{
    // One clean cycle (black/white passes) before the image so no ghosting from the previous picture remains.
    inkplate.burnInClean(1, 0);
    inkplate.display();
}

void Gui::drawScene(const WeatherData &weather)
{
    inkplate.clearDisplay();
    drawBackdrop(inkplate, sceneForWeather(weather.weatherCode), !weather.isDay, 0, 0, kScreenWidth, kScreenWidth);
}

void Gui::wifiError()
{
    gInk = BLACK;
    gPaper = WHITE;
    inkplate.clearDisplay();
    inkplate.setTextColor(gInk);
    inkplate.setFont(&FreeSansBold18pt7b);
    inkplate.setCursor(40, 250);
    inkplate.print("WiFi connection failed.");
    inkplate.setCursor(40, 300);
    inkplate.print("Check credentials.");
    fullRefresh();
}

void Gui::apiError()
{
    gInk = BLACK;
    gPaper = WHITE;
    inkplate.clearDisplay();
    inkplate.setTextColor(gInk);
    inkplate.setFont(&FreeSansBold18pt7b);
    inkplate.setCursor(40, 250);
    inkplate.print("Weather request failed.");
    inkplate.setCursor(40, 300);
    inkplate.print("Will retry on next wake.");
    fullRefresh();
}

// Weather Icons based on open-meteo api code
const uint8_t *Gui::getWeatherIcon(int code)
{
    if (code == 0)
        return icon_s_clear_sky;
    if (code >= 1 && code <= 3)
        return icon_s_partly_cloudy;
    if (code == 45 || code == 48)
        return icon_s_fog;
    if (isRainCode(code))
        return icon_s_rain;
    if (isSnowCode(code))
        return icon_s_snow;
    if (isStormCode(code))
        return icon_s_storm;
    return icon_s_gray;
}

void Gui::drawBattery(int x, int y, int percent)
{
    inkplate.drawRect(x, y, 30, 14, gInk);
    inkplate.fillRect(x + 30, y + 4, 3, 6, gInk);
    int fill = (26 * constrain(percent, 0, 100)) / 100;
    inkplate.fillRect(x + 2, y + 2, fill, 10, gInk);
}

void Gui::drawDashboard(const WeatherData &weather, const Status &status)
{
    // Always black on white.
    gInk = BLACK;
    gPaper = WHITE;

    inkplate.clearDisplay();
    inkplate.setTextColor(gInk);
    inkplate.setTextSize(1);
    inkplate.setTextWrap(false);

    // --- Top bar: date left, battery + update time right ---
    inkplate.setFont(&FreeSansBold18pt7b);
    printHeavy(inkplate, kMargin, 34, dateLine());

    int rightX = kScreenWidth - kMargin;
    rightAligned(inkplate, String(weather.updatedTime), rightX, 34);
    int timeW = textWidth(inkplate, String(weather.updatedTime));
    int cursorX = rightX - timeW - 14;
    if (status.stale)
    {
        inkplate.fillCircle(cursorX, 25, 4, gInk); // dot = data could not be refreshed this time
        cursorX -= 16;
    }
    if (!status.usbPowered && status.batteryPercent >= 0) // battery level only matters when running on battery
    {
        drawBattery(cursorX - 33, 19, status.batteryPercent);
    }
    inkplate.fillRect(kMargin, 46, kScreenWidth - 2 * kMargin, 2, gInk);

    // --- Hero: big temperature, condition, feels-like / wind, and chances if any ---
    inkplate.setFont(&FreeSansBold48pt7b);
    inkplate.setTextSize(2);
    String temp = tempText(weather.currentTemp);
    if (textWidth(inkplate, temp) > 250)
    {
        inkplate.setTextSize(1);
    }
    int tempBase = 190;
    printHeavy(inkplate, kMargin, tempBase, temp);
    int ringX = kMargin + textWidth(inkplate, temp) + 22;
    inkplate.drawCircle(ringX, tempBase - 118, 13, gInk);
    inkplate.drawCircle(ringX, tempBase - 118, 12, gInk);
    inkplate.drawCircle(ringX, tempBase - 118, 11, gInk);
    inkplate.setTextSize(1);

    // Secondary hero text: smaller, regular weight
    inkplate.setFont(&FreeSans18pt7b);
    printText(inkplate, kMargin, 232, weather.weatherDescription, false);

    inkplate.setFont(&FreeSans12pt7b);
    printText(inkplate, kMargin, 264, "Feels " + tempText(weather.feelsLike), false);
    printText(inkplate, kMargin, 288, "Wind " + String((int)roundf(weather.windSpeed)) + (weather.metric ? " km/h" : " mph"), false);

    // Rain / snow / storm chances over the next hours: only the ones that exist, one line, no row of their own.
    int chances[3] = {chanceFor(weather, matchesRain), chanceFor(weather, matchesSnow), chanceFor(weather, matchesStorm)};
    const char *labels[3] = {"Rain ", "Snow ", "Storm "};
    String chanceLine = "";
    for (int i = 0; i < 3; i++)
    {
        if (chances[i] > 0)
        {
            chanceLine += (chanceLine.length() ? "   " : "") + String(labels[i]) + String(chances[i]) + "%";
        }
    }
    int heroBottom = 304;
    if (chanceLine.length())
    {
        inkplate.setFont(&FreeSans12pt7b);
        printText(inkplate, kMargin, 318, chanceLine, false);
        heroBottom = 330;
    }
    inkplate.fillRect(kMargin, heroBottom, kScreenWidth - 2 * kMargin, 2, gInk);

    // --- Hourly strip ---
    int hourW = (kScreenWidth - 2 * kMargin) / WeatherData::kHourlySlots;
    int hy = heroBottom; // top of the hourly section
    for (int i = 0; i < WeatherData::kHourlySlots; i++)
    {
        int colX = kMargin + i * hourW;
        if (!weather.hourlyAvailable[i])
        {
            continue;
        }
        inkplate.setFont(&FreeSans12pt7b); // secondary text: small, regular weight
        centered(inkplate, i == 0 ? String("Now") : String(weather.hourlyTimes[i]), colX, hourW, hy + 26, false);
        inkplate.fillRect(colX + (hourW - 48) / 2 - 2, hy + 32, 52, 52, gPaper); // keep the icon clean over the backdrop
        drawBitmap(inkplate, colX + (hourW - 48) / 2, hy + 34, getWeatherIcon(weather.hourlyWeatherCodes[i]), 48, 48);
        inkplate.setFont(&FreeSansBold18pt7b);
        centeredTemp(inkplate, weather.hourlyTemps[i], colX, hourW, hy + 112, 4, 18);
        if (weather.hourlyPrecip[i] >= 10)
        {
            inkplate.setFont(&FreeSans12pt7b);
            centered(inkplate, String((int)weather.hourlyPrecip[i]) + "%", colX, hourW, hy + 134, false);
        }
    }
    int dayTop = hy + 146;
    inkplate.fillRect(kMargin, dayTop, kScreenWidth - 2 * kMargin, 2, gInk);

    // --- 7-day forecast: day, high, low ---
    int dayW = (kScreenWidth - 2 * kMargin) / 7;
    for (int i = 0; i < 7; i++)
    {
        int colX = kMargin + i * dayW;
        inkplate.setFont(&FreeSans12pt7b);
        centered(inkplate, String(weather.dailyNames[i]), colX, dayW, dayTop + 26, false);
        inkplate.setFont(&FreeSansBold18pt7b);
        centered(inkplate, tempText(weather.dailyMaxTemp[i]), colX, dayW, dayTop + 56);
        inkplate.setFont(&FreeSans18pt7b);
        centered(inkplate, tempText(weather.dailyMinTemp[i]), colX, dayW, dayTop + 84, false);
    }
}
