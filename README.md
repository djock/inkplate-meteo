# Inkplate Meteo

An hourly weather station for the [Soldered Inkplate 4 TEMPERA](https://soldered.com/product/inkplate-4-tempera/) e-paper display, using the free [Open-Meteo](https://open-meteo.com/) API. It's based on Soldered's OpenMeteo weather station example.

The device wakes once an hour, connects to WiFi, fetches the current conditions and forecast, draws them on the display, then goes back into deep sleep.

## Setup

1. Copy `Inkplate4TEMPERA_OpenMeteo_Weather_Station/secrets.example.h` to `secrets.h` in the same folder.
2. Fill in your WiFi credentials, the name and city to display, your UTC offset, and the latitude/longitude to forecast.
3. Open `Inkplate4TEMPERA_OpenMeteo_Weather_Station.ino` in the Arduino IDE, select the **Inkplate 4 TEMPERA** board, and upload.

To use imperial units, set `metricUnits = false` in the sketch.
