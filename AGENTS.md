# Repository Guidelines

## Project Structure & Module Organization
- `Inkplate4TEMPERA_OpenMeteo_Weather_Station/Inkplate4TEMPERA_OpenMeteo_Weather_Station.ino` is the main sketch and entry point.
- `Inkplate4TEMPERA_OpenMeteo_Weather_Station/src/` holds the app modules (`Network.*`, `WeatherData.*`, `Gui.*`).
- Fonts live in `Inkplate4TEMPERA_OpenMeteo_Weather_Station/src/fonts/`.
- Raster assets are in `Inkplate4TEMPERA_OpenMeteo_Weather_Station/src/icons/` with binary icon headers in `Inkplate4TEMPERA_OpenMeteo_Weather_Station/src/binary_Icons/`.

## Build, Test, and Development Commands
- Arduino IDE: open `Inkplate4TEMPERA_OpenMeteo_Weather_Station/Inkplate4TEMPERA_OpenMeteo_Weather_Station.ino`, select board "Inkplate 4 TEMPERA", then Upload.
- Arduino CLI (optional): compile/upload with the Inkplate board FQBN from the Inkplate docs; example flow is `arduino-cli compile` then `arduino-cli upload -p <PORT>`.
- No automated test runner is defined in this repository.

## Coding Style & Naming Conventions
- Indentation: 4 spaces; keep brace style consistent with existing files.
- Types use `PascalCase` (`WeatherData`, `Network::UserInfo`), variables and functions use `camelCase`.
- Keep display assets in `src/icons/` or `src/binary_Icons/`, and keep header includes centralized in `src/includes.h`.
- Avoid committing real WiFi credentials; use placeholders in the sketch when sharing changes.

## Testing Guidelines
- There are no unit or integration tests configured.
- Manual verification: upload to the board and confirm WiFi connect, weather fetch, and display render.
- If you add tests, name them `*_test.cpp` and document how to run them in this file.

## Commit & Pull Request Guidelines
- Use short, imperative commit messages.
- Use short, imperative commit messages (e.g., "Update weather fetch retry logic").
- PRs should describe hardware used (Inkplate 4 TEMPERA), include a brief test note, and attach a screen photo when UI changes are made.

## Configuration & Security Notes
- Copy `secrets.example.h` to `secrets.h` (gitignored) and set WiFi, time zone, latitude and longitude there before flashing.
- NTP server is configurable via `ntpServer` if your network blocks the default pool.
