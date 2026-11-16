# ESP32 Flight Radar

A portable flight radar built on an ESP32 and ST7789 display using live aircraft data from the OpenSky Network.

This project displays nearby aircraft in either a traditional list view or a radar-style view, showing distance, altitude, speed, heading, and movement history. The project was designed for inexpensive ESP32 display boards such as the T-Display and Tenstar ESP32 modules.

---

## Features

### Aircraft Tracking

- Live aircraft data from OpenSky Network
- Automatic refresh interval
- Sort aircraft by nearest distance
- Callsign display
- Altitude display
- Ground speed display
- Heading display

### Radar View

- 10-mile radar range
- Radar scope with compass directions
- Bearing calculation
- Aircraft location plotting
- Altitude-based color coding
- Nearest aircraft highlighting
- Aircraft labels
- Directional aircraft symbols
- Historical movement trails

### List View

- Up to five nearest aircraft
- Distance
- Altitude
- Speed
- Heading

### Device Features

- Battery monitoring
- Battery icon indicator
- Adjustable backlight brightness
- Deep sleep mode
- Wi-Fi auto-connect
- Portable operation

---

## Hardware

### Tested Hardware

- ESP32 Development Board
- ESP32 T-Display
- Tenstar ESP32 Display Board
- ST7789 135x240 Display

### Display Connections

```cpp
#define TFT_CS     5
#define TFT_DC    16
#define TFT_RST   23
#define TFT_MOSI  19
#define TFT_SCLK  18
#define TFT_BL     4
```

### Buttons

```cpp
#define BTN_MODE   35
#define BTN_SCREEN 0
```

---

## Controls

### MODE Button

Short Press

```text
LIST VIEW
    ⇄
RADAR VIEW
```

### DIM / SLEEP Button

Short Press

```text
100%
50%
25%
10%
```

Long Press

```text
Deep Sleep Mode
```

Press the button again to wake the device.

---

## Radar Colors

### Green

```text
Below 5,000 ft
```

### Yellow

```text
5,000 - 20,000 ft
```

### Red

```text
Above 20,000 ft
```

---

## Project Structure

```text
FlightRadar.ino
│
├── Main loop
├── Wi-Fi
├── Battery
├── Buttons
└── Power management

AircraftData.h
│
├── OpenSky API
├── JSON parsing
├── Distance calculations
├── Bearing calculations
├── Aircraft storage
└── Trail history

DisplayUI.h
│
├── Radar rendering
├── List rendering
├── Aircraft symbols
├── Progress bar
├── Battery icon
└── User interface
```

---

## OpenSky API

This project uses the OpenSky Network REST API to retrieve live aircraft state vectors.

OpenSky:
https://opensky-network.org/

Please respect OpenSky's rate limits when changing update intervals.

---

## Future Enhancements

Planned ideas include:

- Aircraft type lookup
- Airport overlays
- Multiple radar ranges
- Aircraft detail screen
- Weather overlays
- GPS-based location
- BLE aircraft alerts
- SD card logging
- Historical playback

---

## Screens

### List View

```text
FLIGHTS

AAL943
4.1mi
12049ft
287mph NW

DAL1034
5.4mi
9200ft
210mph S
```

### Radar View

```text
RADAR

         N

      ✈

W          E

   ✈    ✈

         S
```

---

## License

MIT License

Copyright (c) 2026 Christian Brannon

See LICENSE file for details.
