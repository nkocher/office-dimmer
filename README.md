# ESP32 WiZ Light Dimmer

Control two WiZ A19 smart bulbs with a rotary encoder and ESP32.

## Features

- **Rotary Encoder**: Adjust brightness of both lights (10-100%)
- **Encoder Button**:
  - Click: Cycle color temperature (2200K → 2700K → 4000K → 6500K)
  - Double-click: Toggle both lights on/off
- **Button 1 (GPIO 32)**: Toggle uplight on/off
- **Button 2 (GPIO 33)**: Toggle study lamp on/off

## Hardware Required

- ESP32 DevKit
- HW-040 Rotary Encoder
- 2x Push buttons
- 2x 10kΩ resistors
- Breadboard and jumper wires

## Setup Instructions

### 1. Wiring

See [WIRING.md](WIRING.md) for detailed wiring instructions.

### 2. Software Setup

1. Install [VS Code](https://code.visualstudio.com/)
2. Install [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
3. Open this project folder in VS Code
4. PlatformIO will automatically install dependencies

### 3. Configuration

Copy the secrets template and fill in your values:

```bash
cp src/secrets.h.example src/secrets.h
```

Edit `src/secrets.h` with your WiFi credentials and WiZ bulb IP addresses:

```cpp
const char* ssid = "YOUR_WIFI_SSID";        // Your WiFi name
const char* password = "YOUR_WIFI_PASSWORD"; // Your WiFi password

const IPAddress STUDY_LAMP(192, 168, 0, 159);  // Find in WiZ app
const IPAddress UPLIGHT(192, 168, 0, 55);      // Find in WiZ app
```

### 4. Upload

1. Connect ESP32 via USB
2. Click the PlatformIO upload button (→) in VS Code status bar
3. Open Serial Monitor to see debug output

## Usage

- **Turn encoder**: Adjust brightness (2% per detent)
- **Click encoder**: Cycle color temperature
- **Double-click encoder**: Toggle both lights on/off
- **Press button 1 (GPIO 32)**: Toggle uplight
- **Press button 2 (GPIO 33)**: Toggle study lamp

## How It Works

WiZ bulbs use UDP protocol on port 38899. The ESP32 sends JSON commands:

- Turn on: `{"method":"setPilot","params":{"state":true,"dimming":50}}`
- Turn off: `{"method":"setPilot","params":{"state":false}}`

The rotary encoder uses interrupts for smooth, responsive turning.

## Troubleshooting

**Lights don't respond:**
- Verify lights are on the same WiFi network
- Check IP addresses with WiZ app
- Ensure ESP32 is connected to WiFi (check Serial Monitor)

**Encoder doesn't work:**
- Check CLK/DT wiring (try swapping if direction is reversed)
- Verify 3.3V and GND connections

**Buttons don't work:**
- Check pull-down resistors are connected
- Verify button wiring

## Pin Reference

```
GPIO 25 - Encoder CLK
GPIO 26 - Encoder DT
GPIO 27 - Encoder SW (button)
GPIO 32 - Uplight Button
GPIO 33 - Study Lamp Button
```
