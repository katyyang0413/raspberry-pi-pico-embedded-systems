# Raspberry Pi Pico Embedded Systems

A collection of embedded systems projects developed for the Raspberry Pi Pico (RP2040) using C/C++, ARM assembly, and the Pico SDK.

The projects explore low-level hardware control, sensor interfacing, interrupt handling, multicore execution, and performance analysis on an embedded platform.

---

## Demo

▶ **Demo Video:** [Watch on YouTube / Google Drive]https://youtu.be/DaLJg6RK7xk?si=5Eo06S_x4g3icFgd

---

## Preview

![Raspberry Pi Pico Project Preview](screenshots/preview.png)

---

## Technical Highlights

- Embedded development in C/C++ using the Raspberry Pi Pico SDK
- GPIO configuration and hardware control
- ADC-based internal temperature sensing
- WS2812 RGB LED control using PIO
- Interrupt-driven programming
- Multicore execution across both RP2040 CPU cores
- Flash-cache performance analysis
- ARM Cortex-M0+ assembly integration
- CMake-based build system

---

## Projects

### ADC Temperature Monitoring

Used the RP2040's internal temperature sensor through the ADC interface to read raw sensor values and convert them into temperature measurements.

The application also controls a WS2812 RGB LED to provide a visual indication of temperature.

Key technical details:

- ADC initialization
- Internal temperature sensor enable
- ADC input 4
- Raw ADC acquisition
- Temperature conversion
- WS2812 RGB output through PIO

---

### Interrupt-Driven Applications

Developed applications using hardware interrupts rather than continuously polling inputs.

This included configuring GPIO events and interrupt service routines to react to external hardware events while allowing the processor to perform other work.

---

### Multicore Performance Testing

Investigated the use of both RP2040 processor cores for computational workloads.

A benchmark of 100,000 iterations produced approximately:

| Configuration | Total Execution Time |
|---|---:|
| Single Core | 1.08 s |
| Two Cores | 0.75 s |

Using both cores reduced the total execution time by approximately 30%.

---

### Flash Cache Performance

Measured the effect of disabling the RP2040 flash cache on program execution.

The tests demonstrated a significant increase in execution time when the cache was disabled, highlighting the importance of memory performance in embedded applications.

---

## Hardware

- Raspberry Pi Pico
- RP2040 dual-core ARM Cortex-M0+ microcontroller
- WS2812 RGB LED
- Internal temperature sensor / ADC
- GPIO inputs

---

## Software & Tools

- C
- C++
- ARM Cortex-M0+ Assembly
- Raspberry Pi Pico SDK
- CMake
- VS Code
- Wokwi

---

## Repository Structure

```text
raspberry-pi-pico-embedded-systems/
├── lab03/
├── lab09/
├── lab10/
├── lab11/
├── screenshots/
│   └── preview.png
├── CMakeLists.txt
└── README.md
