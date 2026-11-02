#ifndef GUI_H
#define GUI_H

#include "Inkplate.h"
#include "Scene.h"
#include "WeatherData.h"
#include <Arduino.h>

class Gui
{

public:
    struct Status
    {
        int batteryPercent; // -1 when unknown
        bool usbPowered;
        bool stale; // true when the shown data is older than the latest wake (fetch failed)
    };

    Gui(Inkplate &inkplate);

    // Draw the whole dashboard (backdrop + text) into the frame buffer. Does not refresh the panel.
    // Always black on white.
    void drawDashboard(const WeatherData &weather, const Status &status);
    // Draw the full-screen picture of the outside (dark drawing on white).
    void drawScene(const WeatherData &weather);
    // One full panel refresh with a clean pass first so no ghosting remains.
    void fullRefresh();

    void wifiError();
    void apiError();

private:
    Inkplate &inkplate;
    const uint8_t *getWeatherIcon(int code);
    void drawBattery(int x, int y, int percent);
};

#endif
