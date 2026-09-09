# Raspberry Pi Pico Embedded Systems

This is a project of embedded systems completed as part of my Computer Engineering degree at Trinity College Dublin using the Raspberry Pi Pico (RP2040).

The projects involved programming in C and ARM assembly and working with interrupts, timers, GPIO and other hardware features of the RP2040.

## Morse Code Learning Game

This was a team project where we developed an interactive game for learning Morse code using a Raspberry Pi Pico.

The player enters Morse code using button presses, with short and long presses representing dots and dashes. The game checks the input against the expected character and gives feedback to the player.

Some of the main features include:

- Morse code input using GPIO interrupts
- C and ARM assembly
- Multiple game levels
- Lives system
- WS2812 RGB LED feedback
- PWM buzzer output
- Hardware timers and interrupts
- Watchdog functionality

### My Contribution

My main contributions to the project included:

- Implementing functions for controlling the WS2812 RGB LED
- Working on the lives system and LED colour feedback
- Helping implement the level progression system
- Debugging and fixing build/code issues
- Testing the final game
- Helping prepare and demonstrate the completed project

The source code and project report can be found in the [`morse-code-game`](morse-code-game/) folder.

---

## Interrupt-Driven Alarm

This project focused on using interrupts and external button events on the Raspberry Pi Pico.

A hardware timer interrupt was used to control a flashing LED, while external buttons could change the behaviour of the system.

The buttons were used to:

- Pause and resume the LED
- Increase the flashing rate
- Decrease the flashing rate
- Reset the timing when required

The project used both C and ARM assembly and involved working with GPIO interrupts, hardware timers and the RP2040 interrupt system.

The source code can be found in the [`interrupt-alarm`](interrupt-alarm/) folder.

## Technologies

- Raspberry Pi Pico (RP2040)
- C
- ARM Cortex-M0+ Assembly
- Pico SDK
- GPIO
- Hardware interrupts and timers
- PIO / WS2812 RGB LED
- PWM
- CMake

## Repository Structure

```text
raspberry-pi-pico-embedded-systems/
├── interrupt-alarm/
│   ├── assign01.c
│   ├── assign01.S
│   ├── CMakeLists.txt
│   └── ws2812.pio
│
├── morse-code-game/
│   ├── assign02.c
│   ├── assign02.S
│   ├── CMakeLists.txt
│   ├── D4-report.pdf
│   └── ws2812.pio
│
└── README.md
```
