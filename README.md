
# ESP32 Garage Door Control System

## Overview
This project implements a smart garage door control system using an ESP32. It provides a web interface for controlling and monitoring the garage door, including features such as user authentication, state monitoring, auto-closing, and configuration persistence via NVS storage.

## Features

- **Web Interface:** Control the garage door through a simple web UI.
- **TCP Communication:** Use a home screen widget to open your garage.
- **User Authentication:** Login system with user and admin roles.
- **State Monitoring:** Uses magnetic contacts to determine door state (open, closed, stuck, etc.).
- **Auto-Close Feature:** Configurable automatic closing after a set time or to a set time.
- **Manual Controls:** Toggle, open, and close the door remotely.
- **Error Detection:** Detects garage door beeing stuck and stops it.
- **Over-the-Air (OTA) Updates:** Update firmware without physical access.
- **Logging & Configuration:** Settings are stored in the ESP32's NVS (Non-Volatile Storage).
- **Automatic DDNS Update:** Automatically updates DNS Entry to allow access with a domain.
- 

## Hardware Requirements

- ESP32 microcontroller
- Magnetic contacts for door state detection
- Relay module to control the garage door motor
- Piezo buzzer for warning signals
- Power supply (e.g., 5V or 12V, depending on relay)

## Installation & Setup

Warning: This software is designed to controll a garage door, which trigger line is pulled down on a trigger. Otherwise it won't be able to detect a state-change.

### 1. Flashing the ESP32

1. Install the [Arduino IDE](https://www.arduino.cc/en/software) and set up ESP32 board support.
2. Install necessary libraries:
   - `WiFi.h`
   - `ESPAsyncWebServer.h`
   - `Preferences.h`
   - `ArduinoOTA.h`
   - `ArduinoJson.h`

3. Clone this repository and open the `.ino` file in Arduino IDE.
4. Configure WiFi credentials and more in `secrets.h`.
5. Compile and upload the code to the ESP32.

### 2. Connecting the Hardware

- Connect the magnetic contacts to the designated GPIOs. Place them that each Sensor will give (at least) a short signal when the door reaches it designated spot.
- Wire the relay module to the garage motor trigger.
- Connect the trigger to the esp. Use a voltage divider to ensure the GPIO-Pins won't exeed 3.3V.
- Optionally, connect a piezo buzzer for alerts.

## Web Interface

### Endpoints

| Endpoint        | Method | Description | Admin Only |
|----------------|--------|-------------|--------|
| `/`            | GET    | Main page (redirects to login if not authenticated) | false |
| `/login`       | GET    | Displays login form | false |
| `/login`       | POST   | Processes login credentials | false |
| `/setState`    | POST   | Updates door state | true |
| `/toggle`      | POST   | Toggles garage door | false |
| `/open`        | POST   | Opens the door if closed | false |
| `/close`       | POST   | Closes the door if open | false |
| `/openTime`    | POST   | Opens the door for a set duration | false |
| `/settings`    | POST   | Updates configuration values | false |
| `/read_config` | POST   | Updates Pin configuration of Magnet-Sensors | true |

### Authentication

- Users must log in before accessing the main control panel.
- Users will be signed out five minutes after logging in.
- Admin users have access to additional settings.

## Configuration & Storage

- Uses `Preferences.h` library to store settings in NVS.
- Saves auto-close settings, max open time, and other parameters persistently.

## Troubleshooting

- **Cannot access the web interface?**
  - Check if the ESP32 is connected to WiFi.
  - Ensure correct port forwarding if accessing externally.
- **Settings not saving?**
  - Verify `Preferences.begin()` is correctly used.
  - Ensure sufficient flash memory space is available.

## Future Enhancements

- Add MQTT support for smart home integration.
- Implement a mobile app for easier access.
- Enable TLS encryption for better security.

## License

This project is licensed under the GPL License. Feel free to modify and improve it!

---
For any questions or contributions, feel free to open an issue on GitHub! 🚀
