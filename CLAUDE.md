# ESP32 WiZ Light Dimmer Project

## Project Description

This is an ESP32-based IoT smart lighting controller that operates two WiZ A19 smart bulbs using a rotary encoder and push buttons. The device communicates with WiZ bulbs over WiFi using UDP protocol on port 38899.

**Key Characteristics:**
- Single-file monolithic architecture (src/main.cpp, ~336 lines; credentials in src/secrets.h)
- Physical interface: HW-040 rotary encoder + 2 independent push buttons
- No external modules or class separation (appropriate for embedded scope)
- Fire-and-forget UDP communication (responses drained but not processed)
- Interrupt-based encoder reading for responsive control
- Hardware watchdog (10s timeout) auto-reboots on loop stall
- Non-blocking loop design — no `delay()` calls except the 10ms loop tick

## Development Commands

### Build and Upload

```bash
# Compile and flash to ESP32
pio run -t upload

# Compile only (no upload)
pio run

# Flash and immediately open serial monitor
pio run -t upload && pio device monitor
```

### Monitoring and Debugging

```bash
# Open serial monitor at 115200 baud
pio device monitor -b 115200

# View monitor with filtering (if needed)
pio device monitor -b 115200 | grep -v "noise"
```

**Requirements:**
- VS Code with PlatformIO extension installed
- ESP32 connected via USB
- Serial monitor at 115200 baud for debug output
- Serial output includes WiFi connection status and command logs

## Code Architecture

### Single-File Design

All logic resides in `src/main.cpp` with no module separation:
- **Setup phase** (setup()): Initialize hardware, connect WiFi, configure UDP
- **Main loop** (loop()): 10ms cycle polling encoder/buttons, sending UDP commands
- **Event handlers**: Button callbacks for click/long-press events
- **State management**: Simple global variables (brightness int, boolean flags)

### Key Components

**Libraries Used:**
- `AceButton@^1.10.1` - Event-driven button handling with debouncing and long-press detection
- `ESP32Encoder@^0.10.2` - Interrupt-based rotary encoder support using half-quadrature mode
- `esp_task_wdt.h` - Hardware watchdog timer (ESP-IDF, no PlatformIO dependency needed)
- Built-in: WiFi, WiFiUDP, Arduino core

**State Variables:**
- `brightness` (int): Current brightness level (10-100)
- `studyLampOn` (bool): Study lamp on/off state
- `uplightOn` (bool): Uplight on/off state
- Encoder count tracks brightness in steps of 2%

**GPIO Pin Assignments:**
```
GPIO 25 - Encoder CLK (interrupt-capable)
GPIO 26 - Encoder DT (interrupt-capable)
GPIO 27 - Encoder SW button (INPUT_PULLUP)
GPIO 32 - Button 1 (Uplight control, HIGH when pressed)
GPIO 33 - Button 2 (Study lamp control, HIGH when pressed)
```

### Control Flow

The main loop runs every ~10ms with no blocking calls. Order of operations:

1. **WiFi status check** (every 30s, non-blocking):
   - Logs `[WIFI] Disconnected` / `[WIFI] Reconnected` transitions
   - `WiFi.setAutoReconnect(true)` handles actual reconnection — no manual `WiFi.begin()` calls in loop

2. **Heap monitoring** (every 60s):
   - Logs `[HEAP] Free: / Min ever: / Largest block:` for diagnosing memory issues
   - If `Min ever` stays above ~200KB, heap is healthy
   - If `Largest block` diverges from `Free`, fragmentation is occurring

3. **UDP buffer drain**:
   - WiZ bulbs send response packets — these are consumed and discarded each iteration
   - Prevents lwIP buffer overflow that can destabilize the network stack

4. **Encoder rotation**:
   - Encoder count read every loop iteration
   - Brightness calculated as count × BRIGHTNESS_STEP (2%)
   - Constrained to MIN_BRIGHTNESS (10) to MAX_BRIGHTNESS (100)
   - Commands sent only to lights that are currently on

5. **Button handling**:
   - AceButton library checks for events (click, double-click)
   - Callbacks invoked when events detected
   - Debouncing handled automatically by library

6. **Watchdog reset + 10ms delay**:
   - `esp_task_wdt_reset()` feeds the hardware watchdog each iteration
   - 10ms delay sets the loop cadence

### UDP Communication

- JSON payloads built with `snprintf` into stack-allocated `char[128]` buffers (zero heap allocation)
- Sent via `udp.write()` to bulb IP on port 38899
- Fire-and-forget: no response processing, but responses are drained to prevent buffer buildup

## Configuration Requirements

### Before First Upload

**Copy the secrets template and fill in your values:**
```bash
cp src/secrets.h.example src/secrets.h
```

**Edit `src/secrets.h` with your WiFi credentials and WiZ bulb IP addresses:**
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const IPAddress STUDY_LAMP(192, 168, 0, 159);
const IPAddress UPLIGHT(192, 168, 0, 55);
```

### Finding WiZ Bulb IP Addresses

1. Open WiZ mobile app
2. Tap on each bulb
3. Check device settings for IP address
4. Bulbs must be on same WiFi network as ESP32
5. IPs are hardcoded (no automatic discovery)

**Note:** If bulb IPs change (DHCP reassignment), you must update `src/secrets.h` and re-upload.

## Hardware Setup

### Wiring Overview

See WIRING.md for complete diagrams. Key connections:

**HW-040 Rotary Encoder:**
- CLK → GPIO 25
- DT → GPIO 26
- SW → GPIO 27
- VCC → 3.3V (NOT 5V)
- GND → GND

**Push Buttons (both follow same pattern):**
- One terminal → 3.3V
- Other terminal → GPIO pin AND 10kΩ pull-down resistor to GND

### Critical Hardware Notes

1. **Voltage:** Use 3.3V only. ESP32 GPIO pins are NOT 5V tolerant.
2. **Pull-down resistors:** 10kΩ resistors are required for push buttons to prevent floating inputs.
3. **Interrupt capability:** All selected GPIOs (25, 26, 27, 32, 33) support interrupts, required for encoder functionality.
4. **Encoder type:** HW-040 is a mechanical encoder (not optical) with 20 detents per revolution.

## WiZ Protocol Details

### Communication Specs

- **Protocol:** UDP (fire-and-forget, no acknowledgment)
- **Port:** 38899
- **Format:** JSON commands
- **Encoding:** UTF-8 string

### Command Structure

**Turn on with brightness:**
```json
{"method":"setPilot","params":{"state":true,"dimming":50}}
```

**Turn off:**
```json
{"method":"setPilot","params":{"state":false}}
```

### Brightness Range

- **Valid range:** 10-100 (WiZ protocol limits)
- **Adjustment increment:** 2% per encoder detent
- **Default on startup:** 50%
- Values below 10 or above 100 are constrained by encoder clamping in `loop()`

## Troubleshooting

### No Serial Output

- Verify serial monitor baud rate is exactly 115200
- Press RESET button on ESP32 after opening monitor
- Check USB cable supports data (not charge-only)
- Close and reopen serial monitor after upload
- Try different USB port or cable

### Lights Don't Respond

1. **Check WiFi connection:**
   - Look for "WiFi connected!" in serial output
   - Verify ESP32 IP address is displayed
   - If failed, check SSID/password in `src/secrets.h`

2. **Verify bulb configuration:**
   - Use WiZ app to confirm bulbs are online
   - Check bulb IP addresses match code configuration
   - Ensure bulbs are on same WiFi network as ESP32
   - Try controlling bulbs with WiZ app first

3. **Network debugging:**
   - Serial output shows every command sent (look for "Sent to" lines)
   - Verify commands are being sent (IP and JSON visible)
   - Check router isn't blocking UDP port 38899

### Encoder Issues

**Wrong rotation direction:**
- Swap CLK and DT pin assignments in code (`ENCODER_CLK` / `ENCODER_DT` defines)
- Re-upload firmware

**No response to rotation:**
- Verify 3.3V power is connected to encoder VCC
- Check GND connection
- Confirm encoder is seated firmly in breadboard
- Test with multimeter: CLK and DT should pulse when turned

**Erratic behavior:**
- Add 0.1µF capacitors between CLK/GND and DT/GND for debouncing (hardware solution)
- Encoder uses internal weak pull-ups (`ESP32Encoder::useInternalWeakPullResistors = UP`)

### Button Issues

**Button doesn't trigger:**
- Confirm 10kΩ pull-down resistors are installed
- Test button with multimeter: should read 3.3V when pressed, 0V when released
- Check wiring matches WIRING.md exactly
- Verify button orientation (some buttons have directional terminals)

**Multiple triggers per press:**
- AceButton library handles debouncing automatically
- If issues persist, check for loose breadboard connections

### Buttons/Encoder Stop Responding After Hours

This was caused by three issues (all fixed):
1. **Blocking WiFi reconnection** — `WiFi.disconnect()` + `WiFi.begin()` in the loop stalled for seconds. AceButton does not buffer events, so presses during the stall were lost. Fix: removed manual reconnection, rely on `WiFi.setAutoReconnect(true)`.
2. **UDP receive buffer overflow** — WiZ response packets were never consumed. Fix: drain with `udp.parsePacket()` + `udp.flush()` each iteration.
3. **Heap fragmentation** — `String` concatenation allocated/freed heap on every command. Fix: replaced with stack-allocated `char[128]` + `snprintf`.

If the problem recurs, check `[HEAP]` serial output. Steady decline in `Min ever` indicates a memory leak. Hardware watchdog will auto-reboot after 10s of loop stall.

### Encoder Button (SW) Issues

**No response to click:**
- Encoder SW uses INPUT_PULLUP mode (init with HIGH)
- This pin should read HIGH when not pressed, LOW when pressed
- Check encoder is properly seated

## Code Modification Patterns

### Adding a New Light

1. **Add IP address constant** in `src/secrets.h`:
   ```cpp
   const IPAddress NEW_LIGHT(192, 168, 0, 100);
   ```

2. **Add state variable** (near existing state variables in main.cpp):
   ```cpp
   bool newLightOn = false;
   ```

3. **Create AceButton instance** (near existing AceButton declarations):
   ```cpp
   AceButton buttonNewLight;
   ```

4. **Add button handler function** (after existing handlers at end of file):
   ```cpp
   void handleNewLightButton(AceButton* button, uint8_t eventType, uint8_t buttonState) {
     if (eventType == AceButton::kEventClicked) {
       newLightOn = !newLightOn;
       Serial.print("New Light: ");
       Serial.println(newLightOn ? "ON" : "OFF");
       sendWizCommand(NEW_LIGHT, newLightOn, brightness);
     }
   }
   ```

5. **Initialize button in setup()** (near existing button pin setup):
   ```cpp
   pinMode(BUTTON_NEW_LIGHT, INPUT);
   buttonNewLight.init(BUTTON_NEW_LIGHT, HIGH);
   ```

6. **Configure handler** (near existing button config blocks):
   ```cpp
   ButtonConfig* newLightConfig = buttonNewLight.getButtonConfig();
   newLightConfig->setEventHandler(handleNewLightButton);
   newLightConfig->setFeature(ButtonConfig::kFeatureClick);
   ```

7. **Check button in loop()** (near existing `buttonX.check()` calls):
   ```cpp
   buttonNewLight.check();
   ```

8. **Update encoder brightness control** (in the encoder rotation section of loop()):
   ```cpp
   if (newLightOn) {
     sendWizCommand(NEW_LIGHT, true, brightness);
   }
   ```

9. **Optionally update encoder toggle** (in handleEncoderButton() double-click case):
   ```cpp
   newLightOn = !newLightOn;
   sendWizCommand(NEW_LIGHT, newLightOn, brightness);
   ```

### Changing Pin Assignments

1. Update `#define` statements at top of main.cpp
2. Update WIRING.md with new pin assignments
3. Update README.md pin reference section
4. Physically rewire hardware to match new assignments
5. Re-upload firmware

**Important:** Ensure new pins are interrupt-capable for encoder (most ESP32 pins are, except GPIO 6-11).

### Adjusting Brightness Range or Steps

**Change brightness limits** (constants near top of main.cpp):
```cpp
const int MIN_BRIGHTNESS = 20;   // Raise minimum
const int MAX_BRIGHTNESS = 100;  // Keep maximum
const int BRIGHTNESS_STEP = 10;  // Larger steps (10% instead of 5%)
```

**Effect:** Larger steps mean fewer encoder turns for full range. WiZ protocol accepts 10-100 only.

### Adding Different Button Actions

AceButton supports multiple event types beyond click:

**Long press example** (already used for encoder button):
```cpp
config->setFeature(ButtonConfig::kFeatureLongPress);
// In handler:
case AceButton::kEventLongPressed:
  // Action here
  break;
```

**Double-click example:**
```cpp
config->setFeature(ButtonConfig::kFeatureDoubleClick);
// In handler:
case AceButton::kEventDoubleClicked:
  // Action here
  break;
```

See AceButton documentation for full event list.

## Known Limitations

1. **WiFi credentials in local file** - Stored in `src/secrets.h` (gitignored), no web configuration interface or WiFi manager
2. **No command acknowledgment** - Fire-and-forget; no way to know if bulb received command (responses are drained but not parsed)
3. **Limited error recovery** - Hardware watchdog reboots on hard hangs; no retry logic for failed UDP sends or offline bulbs
4. **Fixed to 2 lights** - Adding more requires code changes (not scalable via config)
5. **No OTA updates** - Firmware updates require USB connection
6. **Stateless operation** - Loses current on/off state on reboot (brightness persists via encoder count)
7. **No dimming animation** - Brightness changes are instant, not gradual
8. **DHCP vulnerability** - If router reassigns bulb IPs, controller breaks until reconfigured

## Project Structure

```
office-dimmer/
├── src/
│   ├── main.cpp          # All application logic
│   ├── secrets.h         # Real credentials (gitignored, local only)
│   └── secrets.h.example # Credential template (committed)
├── platformio.ini        # Build configuration
├── LICENSE              # MIT license
├── README.md            # User-facing setup instructions
├── WIRING.md            # Hardware wiring diagrams
└── CLAUDE.md           # This file (development guide)
```

## Build Configuration

From platformio.ini:
- **Platform:** espressif32
- **Board:** esp32dev (generic ESP32 DevKit)
- **Framework:** Arduino
- **Monitor speed:** 115200 baud
- **Dependencies:** AceButton 1.10.1, ESP32Encoder 0.10.2

No custom build flags or upload settings required.

## Testing Approach

Manual testing via serial monitor:

1. **WiFi connection:** Look for "WiFi connected!" message and IP address
2. **Encoder rotation:** Turn encoder, verify "Brightness: XX" messages
3. **Button presses:** Press each button, verify "Study Lamp: ON/OFF" or "Uplight: ON/OFF"
4. **UDP commands:** Check "Sent to X.X.X.X [ID:N]: {...}" messages show correct JSON
5. **Physical verification:** Confirm actual lights respond as expected
6. **Heap stability:** Watch `[HEAP]` logs for 30+ minutes — `Min ever` should stay within 5KB of initial `Free`
7. **WiFi resilience:** Disconnect router briefly, confirm buttons remain responsive and `[WIFI]` logs show disconnect/reconnect
8. **Extended run:** Leave running 72+ hours, check for stable heap and consistent button/encoder operation

No automated tests or unit tests in project.

## Serial Output Reference

**Normal startup sequence:**
```
=================================
ESP32 WiZ Dimmer Starting...
=================================
1. Setting up encoder...
   Encoder OK
2. Setting up buttons...
   Pins configured
   Buttons initialized
3. Configuring button handlers...
   Button handlers OK
4. Connecting to WiFi...
   SSID: YourNetwork
..........
WiFi connected!
IP address: 192.168.0.123
Ready! Turn the encoder to adjust brightness.
Press buttons to toggle lights on/off.
```

**During operation:**
```
Brightness: 65
Sent to 192.168.0.XXX [ID:5]: {"id":5,"method":"setPilot","params":{"state":true,"dimming":65}}
Study Lamp: ON
Sent to 192.168.0.XXX [ID:6]: {"id":6,"method":"setPilot","params":{"state":true,"dimming":65}}
```

**Periodic diagnostics (automatic):**
```
[HEAP] Free: 270148  Min ever: 268432  Largest block: 114688
[WIFI] Disconnected — auto-reconnect active
[WIFI] Reconnected! IP: 192.168.0.123
```

## Future Enhancement Ideas

If extending this project, consider:

- WiFi Manager library for easy credential configuration
- MQTT support for home automation integration
- Web interface for remote control
- State persistence using SPIFFS or Preferences library
- OTA (Over-The-Air) firmware updates
- Multiple light groups with different control profiles
- Dimming curves or animation effects
- Status LED indicators for connection/control state
- Scene presets (saved brightness/color combinations)
