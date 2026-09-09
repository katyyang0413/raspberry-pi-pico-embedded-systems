# Specify the name of the executable.
add_executable(assign02
    assign02.c
    assign02.S
)

# Specify the source files to be compiled.
target_sources(assign02 PRIVATE assign02.c assign02.S)

# Generate the PIO header file from the PIO source file.
pico_generate_pio_header(assign02 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Pull in required libraries.
# hardware_watchdog — watchdog_enable, watchdog_update, watchdog_caused_reboot
# hardware_clocks   — clock_get_hz used inside ws2812_program_init
# hardware_pio      — PIO state machine for WS2812 LED
target_link_libraries(assign02 PRIVATE
    pico_stdlib
    hardware_pio
    hardware_clocks
    hardware_watchdog
    hardware_pwm
)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(assign02)

# Add the URL via pico_set_program_url.
#apps_auto_set_url(assign02)