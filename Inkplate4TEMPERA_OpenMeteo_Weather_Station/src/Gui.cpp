#include "Gui.h"
#include "Network.h"
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
#include "binary_Icons/icon_s_thermometer.h"

// all the battery icons
#include "binary_Icons/icon_s_full_battery.h"
#include "binary_Icons/icon_s_half_battery.h"
#include "binary_Icons/icon_s_high_battery.h"
#include "binary_Icons/icon_s_low_battery.h"

// font
#include "fonts/FreeSans12pt7b.h"
#include "fonts/FreeSans18pt7b.h"
#include "fonts/FreeSans24pt7b.h"
#include "fonts/FreeSans9pt7b.h"
#include "fonts/FreeSans32pt7b.h"
#include "fonts/FreeSans48pt7b.h"
#include "fonts/FreeSansBold48pt7b.h"

namespace
{
    const int kScreenWidth = 600;
    const int kTopBarHeight = 60;
    const int kSectionGap = 18;
    const int kCurrentRowHeight = 190;
    const int kDailyRowHeight = 140;
    const int kLeftColumnWidth = 300;
    const int kContentStartY = kTopBarHeight + kSectionGap;
    const int kDailyRowY = kContentStartY + kCurrentRowHeight + kSectionGap;
    const int kHourlyRowY = kDailyRowY + kDailyRowHeight + kSectionGap;

    void drawBitmapScaled(Inkplate &inkplate, int x, int y, const uint8_t *bitmap, int w, int h, int color,
                          int scaleNumer, int scaleDenom)
    {
        int destW = (w * scaleNumer) / scaleDenom;
        int destH = (h * scaleNumer) / scaleDenom;
        int bytesPerRow = (w + 7) / 8;

        for (int dy = 0; dy < destH; dy++)
        {
            int srcY = (dy * scaleDenom) / scaleNumer;
            for (int dx = 0; dx < destW; dx++)
            {
                int srcX = (dx * scaleDenom) / scaleNumer;
                int byteIndex = srcY * bytesPerRow + srcX / 8;
                uint8_t mask = 0x80 >> (srcX & 7);
                if (pgm_read_byte(&bitmap[byteIndex]) & mask)
                {
                    inkplate.drawPixel(x + dx, y + dy, color);
                }
            }
        }
    }

    int getTextWidth(Inkplate &inkplate, const String &text)
    {
        int16_t x1;
        int16_t y1;
        uint16_t w;
        uint16_t h;
        inkplate.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
        return w;
    }

    String truncateTextToWidth(Inkplate &inkplate, const String &text, int maxWidth)
    {
        if (getTextWidth(inkplate, text) <= maxWidth)
        {
            return text;
        }

        String trimmed = text;
        while (trimmed.length() > 0)
        {
            trimmed.remove(trimmed.length() - 1);
            String candidate = trimmed + "...";
            if (getTextWidth(inkplate, candidate) <= maxWidth)
            {
                return candidate;
            }
        }

        return "";
    }

    void drawCenteredText(Inkplate &inkplate, const String &text, int columnX, int columnWidth, int baselineY)
    {
        int textWidth = getTextWidth(inkplate, text);
        int textX = columnX + (columnWidth - textWidth) / 2;
        if (textX < columnX)
        {
            textX = columnX;
        }
        inkplate.setCursor(textX, baselineY);
        inkplate.print(text);
    }

    void drawCenteredTempWithDegree(Inkplate &inkplate, const String &valueText, int columnX, int columnWidth, int baselineY,
                                    int degreeRadius)
    {
        (void)degreeRadius;
        int textWidth = getTextWidth(inkplate, valueText);
        int textX = columnX + (columnWidth - textWidth) / 2;
        if (textX < columnX)
        {
            textX = columnX;
        }

        inkplate.setCursor(textX, baselineY);
        inkplate.print(valueText);
    }

    void drawTempWithDegree(Inkplate &inkplate, const String &valueText, int x, int baselineY, int degreeRadius)
    {
        (void)degreeRadius;
        inkplate.setCursor(x, baselineY);
        inkplate.print(valueText);
    }
}

static String getFormattedDateLine()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return "Date N/A";
    }

    char buffer[40];
    strftime(buffer, sizeof(buffer), "%A %d %b", &timeinfo);
    return String(buffer);
}

Gui::Gui(Inkplate &inkplate) : inkplate(inkplate)
{
}

void Gui::drawBackground()
{
    inkplate.clearDisplay();
    inkplate.display();
    inkplate.clearDisplay();

    // Top date bar
    inkplate.fillRect(0, 0, kScreenWidth, kTopBarHeight, 0);
}

void Gui::wifiError()
{
    inkplate.clearDisplay();
    inkplate.setTextColor(0);
    inkplate.setFont(&FreeSans18pt7b);
    inkplate.setCursor(50, 150);
    inkplate.print("WiFi connection failed.");
    inkplate.setCursor(50, 200);
    inkplate.print("Check credentials or try again.");
    inkplate.display();
}

void Gui::apiError()
{
    inkplate.clearDisplay();
    inkplate.setTextColor(0);
    inkplate.setFont(&FreeSans18pt7b);
    inkplate.setCursor(50, 150);
    inkplate.print("HTTP request failed.");
    inkplate.setCursor(50, 200);
    inkplate.print("Check API URL or try again.");
    inkplate.display();
}

int Gui::voltageToPercentage(double voltage) {
    if (voltage >= 4.2) return 100;
    if (voltage <= 3.0) return 0;
  
    // Simple linear approximation
    return (int)(((voltage - 3.0) / (4.2 - 3.0)) * 100);
}

// Weather Icons based on open-meteo api code
const uint8_t *Gui::getWeatherIcon(int code)
{
    switch (code)
    {
    case 0:
        return icon_s_clear_sky;
    case 1:
    case 2:
    case 3:
        return icon_s_partly_cloudy;
    case 45:
    case 48:
        return icon_s_fog;
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82:
        return icon_s_rain;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
        return icon_s_snow;
    case 95:
    case 96:
    case 99:
        return icon_s_storm;
    default:
        return icon_s_gray;
    }
}

const uint8_t *Gui::getBatteryIcon(int percentage)
{
    if (percentage >= 75)
        return icon_s_full_battery;
    else if (percentage >= 50)
        return icon_s_high_battery;
    else if (percentage >= 25)
        return icon_s_half_battery;
    else
        return icon_s_low_battery;
}

// --- Draw Temperature & Precipitation Graph ---
void Gui::drawTemperaturePrecipGraph(WeatherData *weatherData, Network::UserInfo *userInfo)
{
    // Layout values for graph placement
    int graphX = 60;
    int graphY = 185;
    int graphWidth = 500;
    int graphHeight = 250;

    inkplate.setCursor(10, 185);
    inkplate.setFont(&FreeSans12pt7b);
    inkplate.setTextColor(0);
    inkplate.print("Hourly temperature and precipitation");

    int marginX = 20;
    int marginY = 20;
    int chartLeft = graphX + marginX;
    int chartBottom = graphY + graphHeight - marginY;
    int chartTop = graphY + marginY;
    float actualTempMin = 100, actualTempMax = -100;
    float precipMax = 100;

    // Find actual min/max for temperature and precipitation
    for (int i = 0; i < 6; i++)
    {
        if (weatherData->hourlyTemps[i] < actualTempMin)
            actualTempMin = weatherData->hourlyTemps[i];
        if (weatherData->hourlyTemps[i] > actualTempMax)
            actualTempMax = weatherData->hourlyTemps[i];
    }

    // Add padding but ensure min temperature doesn't go below zero (or another reasonable value)
    float paddedTempMin = actualTempMin - 2;
    float paddedTempMax = actualTempMax + 2;

    // Calculate the mid temperature as the average of min and max
    float paddedTempMid = (paddedTempMin + paddedTempMax) / 2;

    float tempRange = paddedTempMax - paddedTempMin;
    if (tempRange == 0)
        tempRange = 1; // Avoid division by zero
    float xStep = (graphWidth - 2 * marginX) / 5.0;

    // Draw axes
    inkplate.drawLine(chartLeft, chartTop, chartLeft, chartBottom, 0);                   // Y-axis
    inkplate.drawLine(chartLeft, chartBottom, graphX + graphWidth + 20, chartBottom, 0); // X-axis

    // Calculate Y positions for min, mid, and max temperatures
    int yMin = chartBottom - ((paddedTempMin - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
    int yMax = chartBottom - ((paddedTempMax - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
    int yMid = chartBottom - ((paddedTempMid - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);

    // Ensure at least 20px gap between the min, mid, and max temperature labels
    if (abs(yMax - yMin) < 20)
    {
        int offset = 20 - abs(yMax - yMin); // Calculate the gap required
        yMax = yMin + offset;               // Adjust the max label position to create the gap
    }

    if (abs(yMid - yMin) < 20)
    {
        int offset = 20 - abs(yMid - yMin); // Calculate the gap required
        yMid = yMin + offset;               // Adjust the mid label position to create the gap
    }

    if (abs(yMax - yMid) < 20)
    {
        int offset = 20 - abs(yMax - yMid); // Calculate the gap required
        yMax = yMid + offset;               // Adjust the max label position to create the gap
    }

    // Draw the temperature labels (Min, Mid, and Max)
    inkplate.setFont(&FreeSans9pt7b);
    inkplate.setTextColor(0);

    // Draw Min temperature label
    drawTempWithDegree(inkplate, String(paddedTempMin, 1), chartLeft - 70, yMin - 20, 3);

    // Draw Mid temperature label
    drawTempWithDegree(inkplate, String(paddedTempMid, 1), chartLeft - 70, yMid, 3);

    // Draw Max temperature label
    drawTempWithDegree(inkplate, String(paddedTempMax, 1), chartLeft - 70, yMax + 20, 3);

    // Draw precipitation bars
    for (int i = 0; i < 6; i++)
    {
        int x = chartLeft + i * xStep;
        int barHeight = (precipMax > 0) ? (weatherData->hourlyPrecip[i] / precipMax) * (graphHeight - 2 * marginY) : 0;
        int y = chartBottom - barHeight;

        // Draw the precipitation bar
        inkplate.fillRect(x + 5, y + 10, 10, barHeight - 10, 4);

        // Draw precipitation value on top of the bar
        inkplate.setCursor(x + 5, y - 5);
        inkplate.print(weatherData->hourlyPrecip[i], 0); // Show precipitation with 1 decimal place
        inkplate.print("%");
    }

    // Draw temperature line
    for (int i = 0; i < 5; i++)
    {
        int x1 = chartLeft + i * xStep;
        int x2 = chartLeft + (i + 1) * xStep;

        int y1 =
            chartBottom - ((weatherData->hourlyTemps[i] - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);
        int y2 =
            chartBottom - ((weatherData->hourlyTemps[i + 1] - paddedTempMin) / tempRange) * (graphHeight - 2 * marginY);

        inkplate.drawLine(x1, y1, x2, y2, 0);
    }

    // Time labels under X-axis
    inkplate.setTextColor(0);
    for (int i = 0; i < 6; i++)
    {
        int x = chartLeft + i * xStep;
        inkplate.setCursor(x + 3, chartBottom + 20);
        inkplate.print(weatherData->hourlyTimes[i]);
    }
}

// --- Display All Weather Data ---
void Gui::displayWeatherData(WeatherData *weatherData, Network::UserInfo *userInfo)
{
    // Top date bar
    inkplate.setTextColor(7);
    inkplate.setFont(&FreeSans24pt7b);
    String dateLine = getFormattedDateLine();
    int16_t x1;
    int16_t y1;
    uint16_t w;
    uint16_t h;
    inkplate.getTextBounds(dateLine.c_str(), 0, 0, &x1, &y1, &w, &h);
    int dateX = (kScreenWidth - w) / 2;
    if (dateX < 12)
    {
        dateX = 12;
    }
    int dateY = (kTopBarHeight / 2) - (y1 + (h / 2)) + 6;
    inkplate.setCursor(dateX, dateY);
    inkplate.print(dateLine);

    inkplate.setTextColor(0);

    // Row 1: current temp (left) + icon/description (right)
    int currentRowY = kContentStartY;
    int rightColumnX = kLeftColumnWidth + 20;
    int iconSize = 96;
    int iconX = rightColumnX + 40;
    int iconY = currentRowY + 20;

    inkplate.setFont(&FreeSans32pt7b);
    inkplate.setTextSize(2);
    String currentTempText = String(weatherData->currentTemp, 0);
    int currentTempColumnX = 0;
    int currentTempColumnWidth = kLeftColumnWidth;
    int currentTempBaselineY = currentRowY + 140;
    drawCenteredTempWithDegree(inkplate, currentTempText, currentTempColumnX, currentTempColumnWidth,
                               currentTempBaselineY, 8);
    inkplate.setTextSize(1);

    drawBitmapScaled(inkplate, iconX, iconY, getWeatherIcon(weatherData->weatherCode), 48, 48, 0, 2, 1);

    inkplate.setFont(&FreeSans18pt7b);
    inkplate.setTextWrap(false);
    int rightColumnWidth = kScreenWidth - rightColumnX;
    String descText = truncateTextToWidth(inkplate, weatherData->weatherDescription, rightColumnWidth - 10);
    int descWidth = getTextWidth(inkplate, descText);
    int descX = rightColumnX + (rightColumnWidth - descWidth) / 2;
    if (descX < rightColumnX)
    {
        descX = rightColumnX;
    }
    inkplate.setCursor(descX, iconY + iconSize + 28);
    inkplate.print(descText);
    inkplate.setTextWrap(true);

    // Bottom row: next 5 days (letter + temp)
    int dayColumnWidth = 110;
    int daysStartX = (kScreenWidth - (dayColumnWidth * 5)) / 2;
    int dayLabelY = kHourlyRowY + 40;
    int dayTempY = kHourlyRowY + 95;

    inkplate.setFont(&FreeSans18pt7b);
    for (int i = 0; i < 5; i++)
    {
        int dayIndex = i + 1;
        int xPos = daysStartX + i * dayColumnWidth;
        String dayName = weatherData->dailyNames[dayIndex];
        String dayLetter = dayName.length() > 0 ? dayName.substring(0, 1) : "-";

        drawCenteredText(inkplate, dayLetter, xPos, dayColumnWidth, dayLabelY);

        inkplate.setFont(&FreeSans24pt7b);
        String dayTempText = String(weatherData->dailyMaxTemp[dayIndex], 0);
        drawCenteredTempWithDegree(inkplate, dayTempText, xPos, dayColumnWidth, dayTempY, 5);
        inkplate.setFont(&FreeSans18pt7b);
    }

    // Top row: next 5 hours (time + temp)
    int hourColumnWidth = 110;
    int hoursStartX = (kScreenWidth - (hourColumnWidth * 5)) / 2;
    int hourLabelY = kDailyRowY + 40;
    int hourTempY = kDailyRowY + 95;

    for (int i = 0; i < 5; i++)
    {
        int xPos = hoursStartX + i * hourColumnWidth;
        if (weatherData->hourlyAvailable[i])
        {
            String timeText = weatherData->hourlyTimes[i];
            inkplate.setFont(&FreeSans18pt7b);
            drawCenteredText(inkplate, timeText, xPos, hourColumnWidth, hourLabelY);

            inkplate.setFont(&FreeSans24pt7b);
            String hourlyTempText = String(weatherData->hourlyTemps[i], 0);
            drawCenteredTempWithDegree(inkplate, hourlyTempText, xPos, hourColumnWidth, hourTempY, 5);
        }
        else
        {
            inkplate.setFont(&FreeSans18pt7b);
            drawCenteredText(inkplate, "--", xPos, hourColumnWidth, hourLabelY);

            inkplate.setFont(&FreeSans24pt7b);
            drawCenteredText(inkplate, "--", xPos, hourColumnWidth, hourTempY);
        }
    }

    inkplate.display();
}
