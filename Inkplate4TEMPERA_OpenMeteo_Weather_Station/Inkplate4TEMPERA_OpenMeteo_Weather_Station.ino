/*
  Inkplate 4 TEMPERA weather station (Open-Meteo)

  Every 30 minutes the board wakes, fetches fresh weather and draws a picture of what it looks like
  outside. Then it goes back to deep sleep. Everything is dark on white; there is no dark mode.

  Each refresh first shows the full-screen picture of the outside (sun, moon and stars, clouds,
  rain, snow, storm) for a few seconds, then the weather. The side (wake) button refreshes right away.
  If a fetch fails, the last good data is kept and shown with a dot next to the time, and the board
  retries after a few minutes.

  Setup: copy secrets.example.h to secrets.h and fill in WiFi and location.
*/

#ifndef ARDUINO_INKPLATE4TEMPERA
#error "Wrong board selection for this example, please select Inkplate 4 TEMPERA in the boards menu."
#endif

#include "src/includes.h"

// WiFi credentials and location live in secrets.h (gitignored).
#include "secrets.h"

// --- WiFi Configuration ---
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// --- Location ---
float latitude = LATITUDE;
float longitude = LONGITUDE;
bool metricUnits = true; // set to false for Fahrenheit / mph

// POSIX time zone, includes daylight saving rules. Bucharest: EET-2EEST,M3.5.0/3,M10.5.0/4
const char *tzPosix = "EET-2EEST,M3.5.0/3,M10.5.0/4";
const char *ntpServer = "pool.ntp.org";

// --- Schedule ---
const uint32_t refreshSeconds = 30 * 60;
const uint32_t sceneSeconds = 30;     // how long the picture stays on each wake
const uint32_t retrySeconds = 5 * 60; // after a failed fetch

// Side (wake) button of the Inkplate 4 TEMPERA: GPIO 36, active low.
const gpio_num_t wakeButtonPin = GPIO_NUM_36;

// --- Battery indicator ---
// The fuel gauge sometimes needs a moment (or fails) after a reset; the icon is simply hidden then.
const int usbCurrentThresholdMa = 5; // average current above this means the battery is charging
const int usbVoltageThresholdMv = 4150;

// --- Device and Data Objects ---
Inkplate inkplate(INKPLATE_1BIT);
Network network;
Network::UserInfo userInfo;
Gui gui(inkplate);

// Survive deep sleep
RTC_DATA_ATTR WeatherData cachedWeather;
RTC_DATA_ATTR bool lastFetchFailed = false;
RTC_DATA_ATTR int lastBatteryPercent = -1;
RTC_DATA_ATTR int gaugeSkipWakes = 0; // when the gauge failed, skip it for a while: a failing gauge stalls boot for ~45 s
RTC_DATA_ATTR bool lastPowerUsb = true; // last reliable power reading; used when the fuel gauge is not ready

#define uS_TO_S_FACTOR 1000000ULL

struct PowerState
{
    bool usb;
    int percent; // -1 unknown
};

PowerState readPowerState()
{
    PowerState state = {lastPowerUsb, -1};

    if (gaugeSkipWakes > 0)
    {
        gaugeSkipWakes--;
        return state;
    }

    inkplate.wakePeripheral(INKPLATE_FUEL_GAUGE);
    if (!inkplate.battery.begin())
    {
        gaugeSkipWakes = 16;
        return state;
    }

    // Right after a reset the gauge answers with a placeholder (3600 mV, 0%, 0 mA) or 65535.
    // Poll until it reports real values, for up to ~3 s.
    int mv = 0, soc = 0, current = 0;
    bool valid = false;
    for (int attempt = 0; attempt < 15 && !valid; attempt++)
    {
        delay(200);
        mv = inkplate.battery.voltage();
        soc = inkplate.battery.soc();
        current = inkplate.battery.current(AVG);
        bool placeholder = (mv == 3600 && soc == 0 && current == 0);
        valid = mv > 2500 && mv < 5000 && !placeholder;
    }
    Serial.printf("Fuel gauge: %d mV, %d%%, %d mA (%s)\n", mv, soc, current, valid ? "ready" : "not ready");

    if (!valid)
    {
        gaugeSkipWakes = 16; // about 8 hours
    }
    if (valid)
    {
        state.usb = current > usbCurrentThresholdMa || mv >= usbVoltageThresholdMv;
        lastPowerUsb = state.usb;
        state.percent = constrain(soc, 0, 100);
        if (soc == 0 && mv > 3400)
        {
            // gauge has not learned the battery yet: estimate from voltage
            state.percent = constrain((mv - 3200) * 100 / (4150 - 3200), 0, 100);
        }
        lastBatteryPercent = state.percent;
    }
    return state;
}

bool connectAndSyncTime()
{
    const unsigned long timeout = 30000;
    unsigned long start = millis();
    bool connected = false;
    while (!connected && (millis() - start) < timeout)
    {
        connected = inkplate.connectWiFi(ssid, password, 10, true);
    }
    if (!connected)
    {
        return false;
    }
    configTzTime(tzPosix, ntpServer);
    struct tm timeinfo;
    getLocalTime(&timeinfo, 10000); // wait for NTP
    return true;
}

// Fetch and store fresh data in the RTC cache. Returns true on success.
bool refreshWeather()
{
    if (!connectAndSyncTime())
    {
        Serial.println("WiFi failed");
        return false;
    }
    userInfo.useMetric = metricUnits;
    WeatherData fresh = cachedWeather;
    network.fetchWeatherData(&fresh, &userInfo, &latitude, &longitude);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    if (userInfo.apiError)
    {
        return false;
    }
    cachedWeather = fresh;
    return true;
}

void goToSleep(uint32_t seconds)
{
    uint32_t awake = millis() / 1000;
    uint32_t sleepFor = seconds > awake + 30 ? seconds - awake : 30; // keep the wake period steady
    Serial.printf("Sleeping %u s\n", sleepFor);

    // Do not sleep while the button is still held, or the level wake would fire again at once.
    pinMode(wakeButtonPin, INPUT);
    unsigned long held = millis();
    while (digitalRead(wakeButtonPin) == LOW && millis() - held < 3000)
    {
        delay(10);
    }

    esp_sleep_enable_timer_wakeup(sleepFor * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(wakeButtonPin, 0);
    esp_deep_sleep_start();
}

void setup()
{
    Serial.begin(115200);
    inkplate.begin();
    inkplate.frontlight.setState(false);

    setenv("TZ", tzPosix, 1);
    tzset();

    bool buttonWake = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
    Serial.println(buttonWake ? "Button wake: refreshing now" : "Scheduled wake");

    PowerState power = readPowerState();
    Serial.printf("Power: %s, battery %d%%\n", power.usb ? "USB" : "battery", power.percent);

    lastFetchFailed = !refreshWeather();

    if (!cachedWeather.valid)
    {
        // nothing to show yet: report why and retry soon
        if (userInfo.apiError)
        {
            gui.apiError();
        }
        else
        {
            gui.wifiError();
        }
        goToSleep(retrySeconds);
    }

    Gui::Status status = {power.percent, power.usb, lastFetchFailed};
    {
        // Show the picture of the outside full screen for a while, then the weather.
        gui.drawScene(cachedWeather);
        gui.fullRefresh();
        delay(sceneSeconds * 1000UL);
    }
    gui.drawDashboard(cachedWeather, status);
    gui.fullRefresh(5); // extra clean passes so the picture does not ghost into the weather screen

    goToSleep(lastFetchFailed ? retrySeconds : refreshSeconds);
}

void loop()
{
    // Nothing to do here - main logic runs once in setup()
}
