/*
  Inkplate4TEMPERA OpenMeteo Weather Station Example
  Compatible with Soldered Inkplate 4 Tempera

  Getting Started:
  For setup and documentation, visit: https://inkplate.readthedocs.io/en/latest/

  Overview:
  This example demonstrates how to fetch and display weather data from the OpenMeteo API
  using the Inkplate 10 e-paper display.

  Before You Start:
  - Enter your WiFi credentials carefully (they are case-sensitive).
  - Update the following variables for accurate local weather data:
      • timeZone
      • latitude
      • longitude
  Set your username and city with `myUsername` and `myCity` (for display only, not essential for the API).

  Units:
  By default, the app uses the metric system.
  To switch to Imperial units, change the metricUnits to "bool metricUnits = false;"
*/

// Next 3 lines are a precaution, you can ignore those, and the example would also work without them
#ifndef ARDUINO_INKPLATE4TEMPERA
#error "Wrong board selection for this example, please select Inkplate 4 TEMPERA in the boards menu."
#endif

#include "src/includes.h" // Include necessary libraries and dependencies for Inkplate and networking

// WiFi credentials and location live in secrets.h (gitignored).
// Copy secrets.example.h to secrets.h and fill in your values.
#include "secrets.h"

// --- WiFi Configuration ---
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// --- User and Location Info ---
String myUsername = MY_USERNAME; // User's name to be displayed on screen
String myCity = MY_CITY;         // City name for weather data
int timeZone = TIME_ZONE;        // timeZone is the number in (UTC + number) in your time zone UTC + 2 for Osijek, UTC - 4 for New York City
float latitude = LATITUDE;       // Latitude of the city
float longitude = LONGITUDE;     // Longitude of the city

bool metricUnits = true;  // set this to false if you wish to use Imperial units

const char* ntpServer = "pool.ntp.org";  // in case you want to use a different one
const bool frontlightTest = true; // Set to false after verifying frontlight hardware

// --- Device and Data Objects ---
Inkplate inkplate(INKPLATE_3BIT); // Create Inkplate display object (3-bit mode for partial grayscale)
Network network;                  // Network utility for weather fetching
Network::UserInfo userInfo;       // Structure to hold user and device info (battery, last updated, etc.)
WeatherData weatherData;          // Structure to hold fetched weather data
Gui gui(inkplate);                // Drawing visuals and info

// --- Deep Sleep Configuration ---
#define uS_TO_S_FACTOR 1000000 // Convert microseconds to seconds
const uint32_t sleepHours = 1;
const uint32_t timeToSleepSeconds = sleepHours * 3600;

// --- Main Setup: Runs Once on Boot ---
void setup()
{
    Serial.begin(115200);    // Initialize serial monitor for debugging
    inkplate.begin();        // Start the Inkplate display

    if (frontlightTest)
    {
        for (int i = 0; i < 3; i++)
        {
            inkplate.frontlight(true);
            inkplate.setFrontlight(63);
            delay(300);
            inkplate.frontlight(false);
            delay(200);
        }
    }

    inkplate.frontlight(true);
    inkplate.setFrontlight(63);
    delay(50);
    inkplate.clearDisplay(); // Clear the screen

    // Attempt to connect to WiFi
    const unsigned long timeout = 30000;
    unsigned long startTime = millis();
    bool connected = false;

    while (!connected && (millis() - startTime) < timeout)
    {
        connected = inkplate.connectWiFi(ssid, password, 10, true);
    }

    // If WiFi failed, display error message
    if (!connected)
    {
        gui.wifiError();
    }
    else
    {
        configTime(timeZone * 3600, 0, ntpServer); // Set local time via NTP server
        // Gather battery and city info
        gui.voltage = inkplate.readBattery();
        userInfo.city = myCity;
        userInfo.username = myUsername;
        userInfo.useMetric = metricUnits;

        // Fetch weather data for specified coordinates
        network.fetchWeatherData(&weatherData, &userInfo, &latitude, &longitude);

        // Display if weather API call fails
        if (userInfo.apiError)
        {
            gui.apiError();
        }
        else
        {
            // Success: draw UI and data
            gui.drawBackground();
            gui.displayWeatherData(&weatherData, &userInfo);
        }
    }

    // Sleep to save power; wakes once every sleepHours
    esp_sleep_enable_timer_wakeup(timeToSleepSeconds * uS_TO_S_FACTOR);
    esp_deep_sleep_start();                        // Put ESP32 into deep sleep.
}

void loop()
{
    // Nothing to do here - main logic runs once in setup()
}
