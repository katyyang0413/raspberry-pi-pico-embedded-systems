# Specify the name of the executable.
add_executable(assign01)

# Specify the source files to be compiled.
target_sources(assign01 PRIVATE assign01.c assign01.S)

# Generate the PIO header from the ws2812 program.
pico_generate_pio_header(assign01 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Pull in commonly used features.
target_link_libraries(assign01 PRIVATE
    pico_stdlib
    hardware_gpio
    hardware_timer
    hardware_irq
    hardware_pio
    hardware_clocks
)

# Create map/bin/hex file etc.
pico_add_extra_outputs(assign01)

# Add the URL via pico_set_program_url.
apps_auto_set_url(assign01)