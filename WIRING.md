# Wiring Guide for ESP32 Rotary Encoder Dimmer

## Components
- ESP32 DevKit (or similar)
- HW-040 Rotary Encoder
- 2x Push Buttons
- 2x 10kΩ Resistors (for buttons - pull-down resistors)
- Breadboard
- Jumper wires

## Wiring Diagram

### HW-040 Rotary Encoder → ESP32
```
HW-040 Pin    →  ESP32 Pin
─────────────────────────────
CLK           →  GPIO 25
DT            →  GPIO 26
SW            →  GPIO 27 (built-in button on encoder)
+  (VCC)      →  3.3V
GND           →  GND
```

### Button 1 (Uplight Control) → ESP32
```
Button Pin 1  →  3.3V
Button Pin 2  →  GPIO 32
Button Pin 2  →  10kΩ resistor → GND (pull-down)
```

### Button 2 (Study Lamp Control) → ESP32
```
Button Pin 1  →  3.3V
Button Pin 2  →  GPIO 33
Button Pin 2  →  10kΩ resistor → GND (pull-down)
```

## Visual Layout

```
ESP32                          HW-040 Rotary Encoder
┌──────────┐                   ┌─────────────┐
│          │                   │             │
│ 3.3V ────┼───────────────────┤ +           │
│ GND  ────┼───────────────────┤ GND         │
│ GPIO25───┼───────────────────┤ CLK         │
│ GPIO26───┼───────────────────┤ DT          │
│ GPIO27───┼───────────────────┤ SW          │
│          │                   └─────────────┘
│          │
│ 3.3V ────┼─── Button1 ───┬─── GPIO32
│          │                └─── 10kΩ ─── GND
│          │
│ 3.3V ────┼─── Button2 ───┬─── GPIO33
│          │                └─── 10kΩ ─── GND
└──────────┘
```

## Important Notes

1. **Use 3.3V, not 5V**: The HW-040 can work with 3.3V and it's safer for ESP32's GPIO pins which are NOT 5V tolerant.

2. **Pull-down resistors**: The 10kΩ resistors keep the button pins LOW when not pressed, preventing floating inputs.

3. **Interrupt-capable pins**: GPIO 25, 26, 27, 32, and 33 all support interrupts, which is needed for the rotary encoder.

4. **Power**: If you're powering via USB, the ESP32's 3.3V regulator can handle the encoder and buttons easily.

## Assembly Steps

1. Connect ESP32 to breadboard
2. Place rotary encoder on breadboard
3. Connect encoder to ESP32 (5 wires)
4. Place buttons on breadboard
5. Add pull-down resistors for each button
6. Connect buttons to 3.3V and GPIO pins
7. Double-check all connections before powering on
