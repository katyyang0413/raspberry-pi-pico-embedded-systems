# Raspberry Pi Pico Embedded Systems

A collection of embedded systems projects developed for the Raspberry Pi Pico (RP2040) using C/C++, ARM Cortex-M0+ assembly, and the Raspberry Pi Pico SDK.

The repository contains coursework projects covering interrupt-driven programming, GPIO control, timer/alarm handling, ADC temperature sensing, WS2812 RGB LED control, PIO, watchdog functionality, and interactive embedded applications.

---

## Demo

### Morse Code Game Demo

▶ **[Watch the Morse Code Game demo on YouTube](https://youtu.be/DaLJg6RK7xk?si=5Eo06S_x4g3icFgd)**

This video demonstrates the **Morse Code Game** project contained in the `morse-code-game/` folder.
## Projects

### 1. Interrupt-Driven Alarm System

An interrupt-driven application developed for the Raspberry Pi Pico using C and ARM Cortex-M0+ assembly.

The project uses hardware interrupts and timers to control an LED flashing system, with push buttons used to change the system state and flashing rate.

#### Technical Details

- Raspberry Pi Pico / RP2040
- C and ARM Cortex-M0+ assembly
- GPIO input and output
- GPIO interrupt handling
- Hardware timer / alarm interrupts
- Interrupt vector table configuration
- On-board LED control
- WS2812 RGB LED control using PIO
- Push-button input for system control

The system uses three buttons to control its behaviour:

- Pause / resume the flashing sequence
- Decrease the flashing rate
- Increase the flashing rate

Interrupt handling is used instead of continuously polling the buttons, allowing the processor to react directly to hardware events.

📁 Project folder: [`interrupt-alarm/`](interrupt-alarm/)

---

### 2. Morse Code Game

An interactive Morse code game developed using a combination of C and ARM Cortex-M0+ assembly.

The game processes user input through hardware buttons and provides feedback using an RGB LED and buzzer. It includes multiple game states, a lives system, level progression, and reset functionality.

#### Technical Details

- C and ARM Cortex-M0+ assembly
- GPIO interrupts
- Timer / alarm interrupts
- WS2812 RGB LED control using PIO
- PWM buzzer output
- Watchdog functionality
- Button-based Morse code input
- Embedded game-state management
- Low-power `WFI` instruction usage

#### Game Features

- User input is processed as Morse code
- A lives system tracks incorrect answers
- RGB LED colours provide visual feedback
- Three lives are represented through the LED state
- Five consecutive correct answers are required to progress
- Multiple game levels are supported
- Watchdog functionality is used for system recovery
- SOS input can be used to reset the game

The project combines low-level assembly routines with C code using the Pico SDK.

📁 Project folder: [`morse-code-game/`](morse-code-game/)

---

### 3. Temperature ADC + WS2812

A temperature-monitoring application using the RP2040's internal temperature sensor.

The project reads the internal temperature sensor through the ADC, converts the raw ADC measurement into a temperature value, prints the result through the serial terminal, and changes the colour of a WS2812 RGB LED depending on the measured temperature.

#### Technical Details

- RP2040 internal ADC temperature sensor
- ADC initialization
- Internal temperature sensor enable
- ADC input 4
- Raw ADC acquisition
- ADC-to-voltage conversion
- Temperature calculation
- C and ARM Cortex-M0+ assembly integration
- WS2812 RGB LED control using PIO
- Serial output using `printf`

The assembly component initializes the ADC, enables the internal temperature sensor, selects ADC input 4, reads the ADC value, and repeats the measurement approximately once per second.

The C component processes the raw ADC reading and converts it into degrees Celsius using the RP2040 temperature conversion formula.

The RGB LED provides a visual indication of the measured temperature:

| Temperature | LED Colour |
|---|---|
| Below 25°C | Green |
| 25°C to below 30°C | Orange |
| 30°C and above | Red |

> The RP2040 internal temperature sensor measures the temperature of the chip itself and should not be treated as a calibrated room-temperature sensor.

📁 Project folder: [`temperature-adc-ws2812/`](temperature-adc-ws2812/)

---

## Hardware

The projects in this repository use:

- Raspberry Pi Pico
- RP2040 dual-core ARM Cortex-M0+ microcontroller
- Maker Pi Pico
- Push buttons
- WS2812 RGB LED
- On-board LED
- Buzzer
- RP2040 internal temperature sensor

---

## Software and Tools

- C
- C++
- ARM Cortex-M0+ Assembly
- Raspberry Pi Pico SDK
- CMake
- VS Code
- PIO
- Git / GitHub

---

## Project Structure

```text
raspberry-pi-pico-embedded-systems/
├── interrupt-alarm/                  # Interrupt-driven alarm project
│   ├── assign01.c
│   ├── assign01.S
│   ├── ws2812.pio
│   └── CMakeLists.txt
│
├── morse-code-game/                  # Morse code game project
│   ├── CMakeLists.txt                # Build configuration
│   ├── D4-report.pdf                 # Project report
│   ├── Morse-Code-Game-Demo.mp4      # Demo video
│   ├── assign02.S                    # ARM Cortex-M0+ assembly
│   ├── assign02.c                    # Main C source
│   └── ws2812.pio                    # WS2812 PIO program
│
├── temperature-adc-ws2812/           # ADC temperature sensing + WS2812 LED
│   ├── Lab11.c
│   └── Lab11.S
│
├── screenshots/                      # Screenshots used in README
└── README.md
