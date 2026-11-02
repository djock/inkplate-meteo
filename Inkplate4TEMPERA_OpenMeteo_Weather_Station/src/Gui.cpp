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
    const int kMargin = 36;

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
        strftime(buffer, sizeof(buffer), "%a %d %b", &timeinfo);
        return String(buffer);
    }
}

Gui::Gui(Inkplate &inkplate) : inkplate(inkplate)
{
}

void Gui::fullRefresh(int cleanCycles)
{
    // Clean cycles (black/white passes) before the image so no ghosting from the previous picture remains.
    inkplate.burnInClean(cleanCycles, 0);
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

    const int left = kMargin;
    const int right = kScreenWidth - kMargin;
    const int contentW = right - left;

    // --- Top bar: heavy date left, update time right (stale dot, battery), clear of the bezel ---
    inkplate.setFont(&FreeSansBold24pt7b);
    printHeavy(inkplate, left, 64, dateLine());
    String updated = String(weather.updatedTime);
    rightAligned(inkplate, updated, right, 64);
    int cursorX = right - textWidth(inkplate, updated) - 14;
    if (status.stale)
    {
        inkplate.fillCircle(cursorX, 52, 5, gInk); // dot = data could not be refreshed this time
        cursorX -= 18;
    }
    if (!status.usbPowered && status.batteryPercent >= 0) // battery level only matters when running on battery
    {
        drawBattery(cursorX - 33, 44, status.batteryPercent);
    }
    inkplate.fillRect(left, 78, contentW, 3, gInk);

    // --- Hero: big temperature and condition on the left, picture of the outside on the right ---
    drawBackdrop(inkplate, sceneForWeather(weather.weatherCode), !weather.isDay, 300, 90, right - 300, 208);

    inkplate.setFont(&FreeSansBold48pt7b);
    inkplate.setTextSize(2);
    String temp = tempText(weather.currentTemp);
    if (textWidth(inkplate, temp) > 230)
    {
        inkplate.setTextSize(1);
    }
    int tempBase = 222;
    printHeavy(inkplate, left, tempBase, temp);
    int ringX = left + textWidth(inkplate, temp) + 22;
    inkplate.drawCircle(ringX, tempBase - 118, 13, gInk);
    inkplate.drawCircle(ringX, tempBase - 118, 12, gInk);
    inkplate.drawCircle(ringX, tempBase - 118, 11, gInk);
    inkplate.setTextSize(1);

    inkplate.setFont(&FreeSans18pt7b);
    printText(inkplate, left + 4, 270, weather.weatherDescription, false);

    // --- Stat row: feels like, wind, and the strongest rain / snow / storm chance (UV when none) ---
    int chances[3] = {chanceFor(weather, matchesRain), chanceFor(weather, matchesSnow), chanceFor(weather, matchesStorm)};
    const char *chanceLabels[3] = {"RAIN", "SNOW", "STORM"};
    int best = -1;
    for (int i = 0; i < 3; i++)
    {
        if (chances[i] > 0 && (best < 0 || chances[i] > chances[best]))
        {
            best = i;
        }
    }
    String labels[3] = {"FEELS", "WIND", best >= 0 ? String(chanceLabels[best]) : String("UV")};
    String values[3] = {tempText(weather.feelsLike), String((int)roundf(weather.windSpeed)) + (weather.metric ? " km/h" : " mph"),
                        best >= 0 ? String(chances[best]) + "%" : String((int)roundf(weather.uvIndex))};
    int cellW = contentW / 3;
    for (int i = 0; i < 3; i++)
    {
        int cx = left + i * cellW;
        inkplate.setFont(&FreeSans12pt7b);
        printText(inkplate, cx + 8, 328, labels[i], false);
        inkplate.setFont(&FreeSansBold18pt7b);
        printHeavy(inkplate, cx + 8, 358, values[i]);
        if (i == 0)
        {
            inkplate.drawCircle(cx + 8 + textWidth(inkplate, values[i]) + 8, 358 - 17, 4, gInk);
            inkplate.drawCircle(cx + 8 + textWidth(inkplate, values[i]) + 8, 358 - 17, 3, gInk);
        }
        if (i > 0)
        {
            inkplate.fillRect(cx - 6, 310, 2, 54, gInk);
        }
    }
    int hy = 374; // top of the hourly section
    inkplate.fillRect(left, hy, contentW, 2, gInk);

    // --- Hourly strip ---
    int hourW = contentW / WeatherData::kHourlySlots;
    for (int i = 0; i < WeatherData::kHourlySlots; i++)
    {
        int colX = left + i * hourW;
        if (!weather.hourlyAvailable[i])
        {
            continue;
        }
        inkplate.setFont(&FreeSans12pt7b); // secondary text: small, regular weight
        centered(inkplate, i == 0 ? String("Now") : String(weather.hourlyTimes[i]), colX, hourW, hy + 24, false);
        drawBitmap(inkplate, colX + (hourW - 48) / 2, hy + 30, getWeatherIcon(weather.hourlyWeatherCodes[i]), 48, 48);
        inkplate.setFont(&FreeSansBold18pt7b);
        centeredTemp(inkplate, weather.hourlyTemps[i], colX, hourW, hy + 100, 4, 18);
        if (weather.hourlyPrecip[i] >= 10)
        {
            inkplate.setFont(&FreeSans12pt7b);
            centered(inkplate, String((int)weather.hourlyPrecip[i]) + "%", colX, hourW, hy + 120, false);
        }
    }
    int dayTop = hy + 130;
    inkplate.fillRect(left, dayTop, contentW, 2, gInk);

    // --- 7-day forecast: day name, then high / low on one line ---
    int dayW = contentW / 7;
    for (int i = 0; i < 7; i++)
    {
        int colX = left + i * dayW;
        inkplate.setFont(&FreeSans12pt7b);
        centered(inkplate, String(weather.dailyNames[i]), colX, dayW, dayTop + 24, false);

        String hi = tempText(weather.dailyMaxTemp[i]);
        String lo = tempText(weather.dailyMinTemp[i]);
        inkplate.setFont(&FreeSansBold18pt7b);
        int hiW = textWidth(inkplate, hi);
        inkplate.setFont(&FreeSans12pt7b);
        int sepW = textWidth(inkplate, "/") + 8;
        int loW = textWidth(inkplate, lo);
        int x = colX + (dayW - (hiW + sepW + loW)) / 2;
        inkplate.setFont(&FreeSansBold18pt7b);
        printHeavy(inkplate, x, dayTop + 52, hi);
        inkplate.setFont(&FreeSans12pt7b);
        printText(inkplate, x + hiW + 4, dayTop + 52, "/", false);
        printText(inkplate, x + hiW + sepW, dayTop + 52, lo, false);
    }
}
