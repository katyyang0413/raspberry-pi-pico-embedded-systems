#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "ws2812.pio.h"

#define IS_RGBW true // using RGBW format from template
#define NUM_PIXELS 1 // only one LED on the board
#define WS2812_PIN 28 // pin connected to RGB LED

// assembly function (entry point from C)
void main_asm(void);

/**
 * sends colour data to the LED using PIO
 */
static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

/**
 * combines r, g, b values into one 32-bit value
 */
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}

/**
 * wrapper functions so assembly can call ADC functions easily
 */
void asm_adc_init(void) {
    adc_init(); // initialise ADC hardware
}

void asm_adc_set_temp_sensor_enabled(int enabled) {
    adc_set_temp_sensor_enabled(enabled); // turn temp sensor on/off
}

void asm_adc_select_input(int input) {
    adc_select_input(input); // choose which ADC input to read
}

uint16_t asm_adc_read(void) {
    return adc_read(); // return raw ADC value
}

void asm_sleep_ms(int ms) {
    sleep_ms(ms); // simple delay
}

/**
 * sets LED colour based on temperature
 */
void set_temperature_colour(float temp_c) {
    // if temp is low → green
    if (temp_c < 25.0f) {
        put_pixel(urgb_u32(0x00, 0x1F, 0x00));
    }
    // medium temp → orange
    else if (temp_c < 30.0f) {
        put_pixel(urgb_u32(0x1F, 0x0F, 0x00));
    }
    // high temp → red
    else {
        put_pixel(urgb_u32(0x1F, 0x00, 0x00));
    }
}

/**
 * converts raw ADC value to temperature and prints it
 */
void process_temperature(uint16_t raw) {
    // convert raw ADC value to voltage
    float voltage = raw * 3.3f / 4096.0f;

    // convert voltage to temperature using formula from datasheet
    float temp_c = 27.0f - (voltage - 0.706f) / 0.001721f;

    // print both raw value and temperature
    printf("Raw ADC: 0x%03X | Temperature: %.6f C\n", raw, temp_c);

    // update LED colour based on temperature
    set_temperature_colour(temp_c);
}

int main(void) {
    // initialise USB output for printf
    stdio_init_all();

    // small delay so terminal connects properly
    sleep_ms(2000);

    // set up PIO for WS2812 LED
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, IS_RGBW);

    // jump to assembly code (this runs the loop)
    main_asm();

    return 0;
}
