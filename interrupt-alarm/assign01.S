#include "hardware/regs/addressmap.h"    
#include "hardware/regs/timer.h"         
#include "hardware/regs/m0plus.h"        

.syntax unified // unified syntax
.cpu cortex-m0plus// cpu type
.thumb // thumb mode
.thumb_func// function is thumb
.global main_asm// make visible to linker
.align 4 // align memory

.equ DFLT_STATE_STRT, 1 // start state
.equ DFLT_STATE_STOP, 0 // stop state
.equ DFLT_ALARM_TIME, 1000000// default alarm time

.equ GPIO_BTN_DN, 20 // down button pin
.equ GPIO_BTN_EN, 21// enter button pin
.equ GPIO_BTN_UP, 22 // up button pin
.equ GPIO_LED_PIN, 25 // built in led pin
.equ GPIO_DIR_IN, 0// input mode
.equ GPIO_DIR_OUT, 1 // output mode

.equ LED_VAL_ON, 1 // led on
.equ LED_VAL_OFF, 0 // led off

.equ ALRM_ISR_OFFSET, 0x40  // alarm interrupt offset

.global main_asm // make visible to linker
.thumb_func // function is thumb
.type main_asm, %function // tell linker this is a function
main_asm:

    ldr r0, =msg_start// load message
    bl printf// print message

    movs r0, #0 // red
    movs r1, #0 // green
    movs r2, #32 // blue
    bl asm_set_colour // set colour

    movs r0, #GPIO_LED_PIN // led pin
    bl asm_gpio_init // init led
    movs r0, #GPIO_LED_PIN// led pin
    movs r1, #GPIO_DIR_OUT  // set output
    bl asm_gpio_set_dir// apply direction
    movs r0, #GPIO_LED_PIN// led pin
    movs r1, #LED_VAL_OFF // led off
    bl asm_gpio_put // write value

    movs r0, #GPIO_BTN_DN// down pin
    bl asm_gpio_init// init
    movs r0, #GPIO_BTN_DN// down pin
    movs r1, #GPIO_DIR_IN// input
    bl asm_gpio_set_dir// set direction
    movs r0, #GPIO_BTN_DN // down pin
    bl asm_gpio_pull_up // enable pull up

    movs r0, #GPIO_BTN_EN // enter pin
    bl asm_gpio_init // init
    movs r0, #GPIO_BTN_EN // enter pin
    movs r1, #GPIO_DIR_IN // input
    bl asm_gpio_set_dir // set direction
    movs r0, #GPIO_BTN_EN // enter pin
    bl asm_gpio_pull_up // enable pull up

    movs r0, #GPIO_BTN_UP // up pin
    bl asm_gpio_init // init
    movs r0, #GPIO_BTN_UP // up pin
    movs r1, #GPIO_DIR_IN // input
    bl asm_gpio_set_dir // set direction
    movs r0, #GPIO_BTN_UP // up pin
    bl asm_gpio_pull_up // enable pull up

    bl isr_install_alarm // install timer interrupt

    cpsie i // enable interrupts

main_loop:
    b main_loop // infinite loop


.global isr_install_alarm // make visible to linker
.thumb_func // function is thumb
.type isr_install_alarm, %function // tell linker this is a function
isr_install_alarm:
    push {lr} // save return

    ldr r0, =PPB_BASE // base address
    ldr r1, =M0PLUS_VTOR_OFFSET // vtor offset
    add r0, r0, r1 // get vtor
    ldr r1, [r0] // load vector table
    ldr r2, =ALRM_ISR_OFFSET // alarm offset
    add r1, r1, r2 // alarm entry
    ldr r2, =alarm_isr // load isr address
    str r2, [r1] // store isr

    ldr r0, =PPB_BASE // base
    ldr r1, =M0PLUS_NVIC_ICPR_OFFSET // clear pending
    add r0, r0, r1 // address
    movs r1, #1 // value
    str r1, [r0] // clear

    ldr r0, =PPB_BASE // base
    ldr r1, =M0PLUS_NVIC_ISER_OFFSET // enable interrupt
    add r0, r0, r1 // address
    movs r1, #1 // value
    str r1, [r0] // enable

    ldr r0, =TIMER_BASE // timer base
    ldr r1, =TIMER_INTE_OFFSET // enable alarm
    add r0, r0, r1 // address
    movs r1, #1 // value
    str r1, [r0] // enable

    bl set_next_alarm // schedule first alarm

    pop {pc}// return


.global alarm_isr // make visible to linker
.thumb_func // function is thumb
.type alarm_isr, %function // tell linker this is a function
alarm_isr:
    push {r0-r5, lr} // save registers

    ldr r4, =lstate // load state
    ldr r5, [r4]// read state
    cmp r5, #DFLT_STATE_STOP// compare stop
    beq alarm_skip// skip if stopped

    bl sub_toggle// toggle led

alarm_skip:
    bl set_next_alarm// schedule next

    ldr r0, =TIMER_BASE// timer base
    ldr r1, =TIMER_INTR_OFFSET // interrupt clear
    add r0, r0, r1 // address
    movs r1, #1 // value
    str r1, [r0] // clear interrupt

    pop {r0-r5, pc} // return


.global set_next_alarm // make visible to linker
.thumb_func // function is thumb
.type set_next_alarm, %function // tell linker this is a function
set_next_alarm:
    push {lr} // save return

    ldr r0, =TIMER_BASE// timer base
    ldr r1, =TIMER_TIMELR_OFFSET // current time
    add r0, r0, r1// address
    ldr r2, [r0] // read time

    ldr r3, =ltimer // load delay
    ldr r3, [r3] // read delay
    add r2, r2, r3 // add delay

    ldr r0, =TIMER_BASE // timer base
    ldr r1, =TIMER_ALARM0_OFFSET // alarm register
    add r0, r0, r1 // address
    str r2, [r0] // set alarm

    pop {pc}// return


.global gpio_isr // make visible to linker
.thumb_func // function is thumb
.type gpio_isr, %function // tell linker this is a function
gpio_isr:
    push {r1-r7, lr}// save registers

    cmp r0, #GPIO_BTN_EN // check enter
    beq handle_toggle// if enter

    cmp r0, #GPIO_BTN_DN // check down
    beq handle_slower // if down

    cmp r0, #GPIO_BTN_UP // check up
    beq handle_faster // if up

    b gpio_done // else exit


handle_toggle:
    ldr r1, =lstate// load state
    ldr r2, [r1]// read state
    movs r3, #1 // value 1
    eors r2, r3 // toggle state
    str r2, [r1] // store state

    cmp r2, #DFLT_STATE_STOP // check stop
    beq paused_colour // if stop

    movs r0, #0 // red
    movs r1, #32 // green
    movs r2, #0 // blue
    bl asm_set_colour // set colour
    b gpio_done                          // exit

paused_colour:
    movs r0, #32 // red
    movs r1, #0 // green
    movs r2, #0 // blue
    bl asm_set_colour // set colour
    b gpio_done // exit


handle_slower:
    ldr r1, =ltimer// load timer
    ldr r2, [r1] // read value
    lsls r2, r2, #1 // multiply by 2
    str r2, [r1] // store

    movs r0, #32 // red
    movs r1, #32 // green
    movs r2, #0 // blue
    bl asm_set_colour // set colour
    b gpio_done // exit


handle_faster:
    ldr r1, =ltimer // load timer
    ldr r2, [r1] // read value
    lsrs r2, r2, #1 // divide by 2
    cmp r2, #0 // check zero
    bne store_fast // if not zero
    movs r2, #1 // set minimum 1
store_fast:
    str r2, [r1] // store
    movs r0, #0 // red
    movs r1, #32 // green
    movs r2, #32 // blue
    bl asm_set_colour // set colour

gpio_done:
    pop {r1-r7, pc} // return


.global sub_toggle // make visible to linker
.thumb_func // function is thumb
.type sub_toggle, %function // tell linker this is a function
sub_toggle:
    push {lr} // save return

    movs r0, #GPIO_LED_PIN // led pin
    bl asm_gpio_get // read led
    cmp r0, #LED_VAL_OFF // check off
    beq led_on // if off turn on

    movs r1, #LED_VAL_OFF // set off
    b led_set  // jump

led_on:
    movs r1, #LED_VAL_ON // set on

led_set:
    movs r0, #GPIO_LED_PIN // led pin
    bl asm_gpio_put // write value

    pop {pc}  // return


.align 4 // align
msg_start: .asciz "Assignment #01 (ASM): start\n"  // message

.data
.align 4
lstate: .word DFLT_STATE_STRT // current state
ltimer: .word DFLT_ALARM_TIME // timer value