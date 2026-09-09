.syntax unified
.cpu cortex-m0plus
.thumb

.global main_asm
.thumb_func

.extern asm_adc_init
.extern asm_adc_set_temp_sensor_enabled
.extern asm_adc_select_input
.extern asm_adc_read
.extern asm_sleep_ms
.extern process_temperature

main_asm:
    push {lr} @ save return address just in case

    bl asm_adc_init @ set up the ADC

    movs r0, #1
    bl asm_adc_set_temp_sensor_enabled @ turn on the internal temperature sensor

    movs r0, #4
    bl asm_adc_select_input @ choose ADC input 4 (this is the temp sensor)

loop:
    bl asm_adc_read @ read raw value from ADC (result goes into r0)

    bl process_temperature @ send raw value to C function to convert + print

    ldr r0, =1000
    bl asm_sleep_ms @ wait 1 second before next reading

    b loop @ go back and repeat forever
