#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

#define GPIO_BTN_DN 20 //button down pin
#define GPIO_BTN_EN 21 //button enter pin
#define GPIO_BTN_UP 22 //button up pin

void gpio_isr(uint gpio);   //assembly interrupt function

void gpio_callback(uint gpio, uint32_t events) {
    (void)events;  //ignore events for now, we only care about the GPIO pin number
    gpio_isr(gpio); //call the assembly interrupt function
} 

void enable_gpio_irq(void) {

    gpio_set_irq_enabled_with_callback(
        GPIO_BTN_EN,   //use this pin for callback
        GPIO_IRQ_EDGE_FALL, //trigger on falling edge (button press)
        true,   //enables interrupt
        gpio_callback //callback function to call when interrupt occurs
    );

    gpio_set_irq_enabled(GPIO_BTN_DN, GPIO_IRQ_EDGE_FALL, true); //enable interrupt
    gpio_set_irq_enabled(GPIO_BTN_UP, GPIO_IRQ_EDGE_FALL, true);
}

void main_asm();    //assembly main function, defined in assign01.s

void asm_gpio_init(uint pin) { gpio_init(pin); }    //initialize GPIO pin, used for button inputs
void asm_gpio_set_dir(uint pin, bool out) { gpio_set_dir(pin, out); } //set GPIO pin direction, used for button inputs
bool asm_gpio_get(uint pin) { return gpio_get(pin); } //read pin
void asm_gpio_put(uint pin, bool value) { gpio_put(pin, value); }//write pin
void asm_gpio_pull_up(uint pin) { gpio_pull_up(pin); }      //enable pull-up resistor on pin, used for button inputs


#define WS2812_PIN 28 // led data pin
#define WS2812_FREQ 800000 // led frequency

static PIO pio = pio0; // use PIO instance 0
static uint sm = 0; // use state machine 0
static uint offset; // offset of the program in instruction memory, assigned during initialization

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b; //grb value
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u); //send data to led
}

void asm_ws2812_init(void) {
    offset = pio_add_program(pio, &ws2812_program); //load pio program into instruction memory and get offset
    ws2812_program_init(pio, sm, offset, WS2812_PIN, WS2812_FREQ, false); //init led
    put_pixel(urgb_u32(0, 0, 0)); //turn off led
}

void asm_set_colour(uint8_t r, uint8_t g, uint8_t b) {
    put_pixel(urgb_u32(r, g, b)); //set led colour
}

int main() {

    stdio_init_all(); //enable stdio for debugging output
    printf("Assignment #01...\n");

    asm_ws2812_init();
    enable_gpio_irq();

    main_asm();

    return 0;
}