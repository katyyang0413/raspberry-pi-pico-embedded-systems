/**
 * @file    assign02.S
 * @brief   ARM Cortex-M0+ assembly implementation of a Morse code input game.
 *
 * Handles GPIO button interrupts for Morse code dot/dash input, alarm timer
 * interrupts for timing and spacing, level selection, game state management,
 * and ISR installation for the RP2040 microcontroller.
 */

#include "hardware/regs/addressmap.h"
#include "hardware/regs/io_bank0.h"
#include "hardware/regs/timer.h"
#include "hardware/regs/m0plus.h"

.syntax unified                                                 @ Specify unified assembly syntax
.cpu    cortex-m0plus                                           @ Specify CPU type is Cortex M0+
.thumb                                                          @ Specify thumb assembly for RP2040
.global main_asm                                                @ Provide program starting address to the linker
.align 4                                                        @ Specify code alignment

/**  Game Constants  */
.equ    DFLT_STATE_STRT, 1                                      @ Specify the value to start flashing
.equ    DFLT_STATE_STOP, 0                                      @ Specify the value to stop flashing
.equ    DFLT_ALARM_TIME, 1000000                                @ Specify the default alarm timeout (1 second in microseconds)
.equ    SLEEP_TIME, 5000                                        @ Sleep duration in milliseconds

.equ    GPIO_BTN_RISE_MSK, 0x00400000                           @ Bit-22 for falling-edge (press) on GP21
.equ    GPIO_BTN_FALL_MSK, 0x00800000                           @ Bit-23 for rising-edge (release) on GP21

/** @brief Threshold in milliseconds separating a dot input from a dash input.
 *         Presses shorter than DOT ms are dots; DOT ms or longer are dashes.
 */
.equ    DOT, 255

.equ    GPIO_BTN,  21                                           @ Specify pin for the button
.equ    GPIO_LED_PIN, 28                                        @ Specify pin for the built-in LED
.equ    GPIO_DIR_IN,   0                                        @ Specify input direction for a GPIO pin
.equ    GPIO_DIR_OUT,  1                                        @ Specify output direction for a GPIO pin

.equ    GPIO_ISR_OFFSET, 0x74                                   @ GPIO is int #13 (vector table entry 29)
.equ    ALRM_ISR_OFFSET, 0x40                                   @ ALARM0 is int #0 (vector table entry 16)

/**
 * @brief   Program entry point. Initialises hardware, then runs the main game loop.
 *
 * Installs GPIO and alarm ISRs, then enters a restart/level-select/gameplay
 * loop. On each restart the game state is reset, the player selects a level
 * via Morse code, and questions are presented until the level is complete,
 * the game is won, or the game is lost.
 *
 * @note    r6 is used throughout as the "currentQuestion / input-pending" flag:
 *          1 = waiting for a new question or level selection, 0 = input received.
 */
.thumb_func
main_asm:
    bl      init_btns                                           @ Use asm_gpio_set_irq C function to setup rising and falling-edge interrupts
    bl      install_gpio_isr                                    @ initialise buttons
    bl      install_alrm_isr                                    @ initialise alarm interrupts

/**
 * @brief   Restart label — resets all game state and re-enters level selection.
 *
 * Jumped to whenever the player loses all lives, enters an SOS reset sequence,
 * or selects an invalid level.
 */
restart_game:
    movs    r2, #0
    movs    r6, #1                                              @ Set currentQuestion=true
    bl      gameStart                                           @ reset lives/counters/LED on every restart
    bl      display_level_choosing                              @ Display message for level choosing
    bl      level_select_true                                   @ Set selecting_level=true
 
/**
 * @brief   Spin-loop that blocks until the player has entered a valid level number.
 *
 * Calls @ref level_input on every iteration; @ref level_input clears r6 when a
 * complete Morse sequence has been decoded, causing the loop to exit.
 */
level_not_chosen:
    bl      level_input                                         @ Enter the level number in morse code   
    cmp     r6, #0
    bne     level_not_chosen                                    @ Wait until an input entered
    bl      level_select_false

/**
 * @brief   Main gameplay loop entry point.
 *
 * Checks r6 to decide whether a new question must be displayed, dispatches to
 * the appropriate level-question routine, then waits for user input via WFI
 * before checking for level completion.
 *
 * @note    Falls through to @ref end when get_level returns 5 (all levels done).
 */
Start:
    cmp     r6, #0                                              @ check if currentQuestion=true
    bne     skip                                                @ if true skip this portion of code
    movs    r6, #1                                              @ else 
    bl      initalizeInputArray                                 @ Initialize array
    bl      get_level                                           @ Get current level
    cmp     r0, #1
    beq     if_level1                                     
    cmp     r0, #2
    beq     if_level2
    cmp     r0, #3
    beq     if_level3
    cmp     r0, #4
    beq     if_level4
    cmp     r0, #5
    beq     end

if_level1:
    bl      level_1_question                                    @ Display level 1 question                         
    b       finish_level_choosing
if_level2:
    bl      level_2_question                                    @ Display level 2 question
    b       finish_level_choosing
if_level3:
    bl      level_3_question
    b       finish_level_choosing
if_level4:
    bl      level_4_question

/**
 * @brief   Clears shared-memory input state after a question has been displayed.
 *
 * Resets @c isr_input to 3 (sentinel "no input") and @c timer_ticks to 0 so
 * the alarm ISR and GPIO ISR start from a clean slate for the new question.
 */
finish_level_choosing:
    ldr     r0, =isr_input                                      @ Clear shared memory isr_input
    movs    r1, #3
    str     r1, [r0]
    ldr     r0, =timer_ticks                                    @ Clear shared memory timer_ticks
    movs    r1, #0
    str     r1, [r0]

/**
 * @brief   WFI pause and input dispatch.
 *
 * Feeds the watchdog, sleeps until an interrupt fires, then processes any
 * pending input from shared memory. Afterwards checks whether the current
 * level is complete, advancing, restarting, or looping accordingly.
 */
skip:
    bl      watchdog_update                                     @ Feed watchdog during gameplay
    wfi                                                         @ wait for alarm/gpio interrupt
    bl      user_input                                          @ check the user input from shared memory

skip2:
    bl      check_level_complete
    cmp     r0, #1
    beq     level_complete
    cmp     r0, #2
    beq     restart_game
    b       Start                                               @ loop back up to start

end:
/**
 * @brief   Dispatches to the correct completion handler for the finished level.
 *
 * Reads the current level via get_level, then advances the level counter and
 * displays a success message before re-entering the main game loop.
 */
level_complete:
    bl      get_level
    cmp     r0, #1
    beq     level1_complete
    cmp     r0, #2
    beq     level2_complete
    cmp     r0, #3
    beq     level3_complete
    cmp     r0, #4
    beq     level4_complete

level1_complete:
    bl      level_success
    movs    r0, #2
    bl      set_level
    b       finish_level
level2_complete:
    bl      level_success
    movs    r0, #3
    bl      set_level
    b       finish_level
level3_complete:
    bl      level_success
    movs    r0, #4
    bl      set_level
    b       finish_level
level4_complete:
    bl      level_success
    movs    r0, #5 
    bl      set_level
    b       finish_level

/**
 * @brief   Resets per-level state and jumps back to the Start loop.
 */
finish_level:
    bl      gameStart
    b       Start

/**
 * @brief   Displays the game-won screen, then restarts the game.
 */
game_won:
    bl      game_won_display
    b       restart_game

/**
 * @brief   Displays the game-over screen, then restarts the game.
 */
game_over: 
    bl      game_over_display             
    b       restart_game

/**
 * @brief   Collects a Morse code level-selection input from the player.
 *
 * Initialises the input array and shared-memory state, then spins on WFI
 * until two alarm ticks have elapsed (indicating the player has finished
 * entering characters). Validates the decoded level (1–4); level 5 input is
 * treated as invalid. On a valid selection the level is committed via
 * set_level and r6 is cleared to signal completion to the caller.
 *
 * An SOS reset sequence detected mid-selection causes an immediate jump to
 * @ref restart_game.
 *
 * @note    Modifies r6 (sets to 0) when a valid level has been entered.
 */
level_input:
    push    {lr}                                                

    bl      initalizeInputArray                                 
    
    ldr     r0, =isr_input                                      
    movs    r1, #3
    str     r1, [r0]
    ldr     r0, =timer_ticks
    movs    r1, #0
    str     r1, [r0]
    
/**
 * @brief   Inner wait loop for level input — blocks until two timer ticks occur.
 */
input_not_complete:
    bl      watchdog_update                                     @ Feed the watchdog while waiting for level select
    wfi                                                         
    bl      user_input                                          
    
    @ Check if SOS reset was requested during level selection
    bl      is_reset_requested
    cmp     r0, #1
    beq     sos_restart_from_level_select

    ldr     r0, =isr_input                                      
    ldr     r1, [r0]
    cmp     r1, #3
    beq     skip3                                               
    movs    r1, #0                                              
    str     r1, [r0]
skip3:
    ldr     r0, =timer_ticks                                    
    ldr     r1, [r0]
    cmp     r1, #2                                              @ Two ticks signals end of Morse input
    bne     input_not_complete

    bl      get_level                                           @ Fetch the updated level from C

    cmp     r0, #1
    beq     level1
    cmp     r0, #2
    beq     level2
    cmp     r0, #3
    beq     level3
    cmp     r0, #4
    beq     level4
    cmp     r0, #5
    beq     level5

/** @cond INTERNAL — level-selection local branches */
level1:
    ldr     r0, =1  
    b       level_selection_input_entered

level2:
    ldr     r0, =2  
    b       level_selection_input_entered

level3:
    ldr     r0, =3  
    b       level_selection_input_entered

level4:
    ldr     r0, =4     
    b       level_selection_input_entered

/**
 * @brief   Handles an out-of-range level input (level 5 selected during setup).
 *          Displays an error message and restarts the game.
 */
level5:
    bl      level_selection_invalid
    b       restart_game

/**
 * @brief   Handles an SOS reset request detected during level selection.
 *          Clears the reset flag and jumps to restart_game.
 */
sos_restart_from_level_select:
    bl      clear_reset_request
    b       restart_game
/** @endcond */

/**
 * @brief   Commits the selected level and signals the caller that input is done.
 *
 * @pre     r0 contains the validated level number (1–4).
 * @post    r6 == 0, indicating to the outer loop that selection is complete.
 */
level_selection_input_entered:
    bl      set_level                                           
    bl      initalizeInputArray                                 
    movs    r6, #0

    pop     {pc}                                                

/**
 * @brief   Reads decoded Morse input from shared memory and calls detectInput.
 *
 * Examines @c timer_ticks and @c isr_input written by the ISRs:
 *  - @c timer_ticks == 1 → inter-character space detected, calls detectInput(3)
 *  - @c timer_ticks == 2 → word/enter detected, calls detectInput(4) and clears r6
 *  - @c isr_input   == 1 → dot received, calls detectInput(1)
 *  - @c isr_input   == 2 → dash received, calls detectInput(2)
 *
 * @note    r6 is intentionally excluded from the push/pop list so that the
 *          "enter" path can clear it and the change is visible to the caller.
 */
user_input:
    push    {r4, r5, r7, lr}                                    @ Protect registers (removed r6 so main loop detects the change)

    ldr     r0, =timer_ticks
    ldr     r5, [r0]                                            
    ldr     r0, =isr_input
    ldr     r4, [r0]                                            

    cmp     r5, #1                                              
    beq     add_space                                           
    cmp     r5, #2                                              
    beq     enter                                               
    cmp     r4, #1                                              
    beq     dot                                                 
    cmp     r4, #2                                              
    beq     dash                                                
    b       input_entered                                       

dot:
    ldr     r1, =isr_input                                      
    movs    r2, #0
    str     r2, [r1]
    ldr     r0, =1                                              @ detectInput argument: 1 = dot
    bl      detectInput                                         
    b       input_entered                                       

dash:
    ldr     r1, =isr_input                                      
    movs    r2, #0
    str     r2, [r1]
    ldr     r0, =2                                              @ detectInput argument: 2 = dash
    bl      detectInput                                         
    b       input_entered                                       

add_space:
    ldr     r0, =3                                              @ detectInput argument: 3 = space
    bl      detectInput                                         
    b       input_entered                                       

enter:
    ldr     r0, =4                                              @ detectInput argument: 4 = enter/submit
    bl      detectInput                                         
    movs    r6, #0                                              @ Signal to main loop that input is complete

input_entered:
    pop     {r4, r5, r7, pc}                                    @ Restore registers (removed r6)

/**
 * @brief   Initialises GPIO_BTN (GP21) as an input with rising- and falling-edge IRQs.
 *
 * Calls the C helpers asm_gpio_init, asm_gpio_set_dir, and asm_gpio_set_irq in
 * sequence to configure the button pin for both-edge interrupt generation.
 */
init_btns:
    push    {lr}                                                   

    movs    r0, #GPIO_BTN                                       
    bl      asm_gpio_init                                       
    movs    r0, #GPIO_BTN                                       
    movs    r1, #GPIO_DIR_IN                                    
    bl      asm_gpio_set_dir                                    
    movs    r0, #GPIO_BTN                                       
    bl      asm_gpio_set_irq                                    

    pop     {pc}                                                

/**
 * @brief   Enables ALARM0 interrupts and arms the timer for the next 1-second tick.
 *
 * Reads the current low 32-bits of the free-running timer (TIMER_TIMELR),
 * adds DFLT_ALARM_TIME (1 000 000 µs = 1 s), and writes the result to
 * TIMER_ALARM0, which fires an interrupt when the timer reaches that value.
 *
 * @note    Must be called from a context that may be interrupted, as it does
 *          not disable interrupts around the read-modify-write of the alarm
 *          register.
 */
set_alarm:
    ldr     r2, =(TIMER_BASE + TIMER_INTE_OFFSET)               @ Enable ALARM0 interrupt
    movs    r1, #1
    str     r1, [r2]

    ldr     r2, =(TIMER_BASE + TIMER_TIMELR_OFFSET)             @ Read current timer value
    ldr     r3, [r2]
    ldr     r1, =DFLT_ALARM_TIME
    add     r1, r3                                              @ target = now + 1 000 000 µs
    ldr     r2, =(TIMER_BASE + TIMER_ALARM0_OFFSET)             
    str     r1, [r2]                                            @ Arm the alarm
    bx      lr                                                  

/**
 * @brief   Installs alrm_isr into the RAM vector table and enables ALARM0 in the NVIC.
 *
 * Reads the Vector Table Offset Register (VTOR) to locate the RAM vector table,
 * then writes the address of @ref alrm_isr at the ALARM0 slot (offset 0x40).
 * Clears any pending ALARM0 interrupt via NVIC_ICPR before enabling it in
 * NVIC_ISER.
 */
install_alrm_isr:
    ldr     r2, =(PPB_BASE + M0PLUS_VTOR_OFFSET)                @ Read vector table base address
    ldr     r1, [r2]                                
    movs    r2, #ALRM_ISR_OFFSET                                
    add     r2, r1                                              @ Compute ALARM0 vector slot address
    ldr     r0, =alrm_isr                           
    str     r0, [r2]                                @ Write ISR address into vector table

    ldr     r2, =(PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET)           @ Clear pending ALARM0 interrupt
    movs    r1, #1 
    str     r1, [r2]

    ldr     r2, =(PPB_BASE + M0PLUS_NVIC_ISER_OFFSET)           @ Enable ALARM0 interrupt in NVIC
    movs    r1, #1 
    str     r1, [r2]

    bx      lr                                                  

/**
 * @brief   ALARM0 interrupt service routine — advances the timer-tick counter.
 *
 * Clears the ALARM0 interrupt flag in TIMER_INTR. If a GPIO button press is
 * currently in progress (@c press_start != 0) or @c isr_input is already set
 * (== 3), the tick is skipped to avoid corrupting in-flight input timing.
 * Otherwise increments @c timer_ticks (shared with the main loop) and
 * re-arms the alarm for the next 1-second period via @ref set_alarm.
 *
 * @note    @c timer_ticks == 1 signals an inter-character gap;
 *          @c timer_ticks == 2 signals end-of-word / submit.
 */
.thumb_func 
alrm_isr:
    push    {r4-r7, lr}                                         @ Protect registers

    ldr     r2, =(TIMER_BASE + TIMER_INTR_OFFSET)               @ Acknowledge / clear ALARM0 interrupt
    movs    r1, #1
    str     r1, [r2]

    ldr     r0, =press_start                                    
    ldr     r7, [r0]
    cmp     r7, #0                                              @ If press_start != 0, a button press is in progress
    bne     gpio_interrupt_in_progress
    
    ldr     r0, =isr_input                                      
    ldr     r4, [r0]
    cmp     r4, #3                                              @ 3 = sentinel "already processed"
    beq     gpio_interrupt_in_progress

    ldr     r0, =timer_ticks
    ldr     r5, [r0]
    ldr     r3, =1
    add     r5, r5, r3                                          @ Increment tick counter
    str     r5, [r0]
    
    bl      set_alarm                                           @ Re-arm for next tick

gpio_interrupt_in_progress:
    pop     {r4-r7, pc}                                         @ Restore registers

/**
 * @brief   Installs gpio_isr into the RAM vector table and enables the GPIO interrupt.
 *
 * Reads VTOR to locate the RAM vector table, writes the address of @ref gpio_isr
 * at the GPIO slot (offset 0x74, interrupt #13 / vector entry 29). Clears and
 * then enables bit 13 (value 8192) in NVIC_ICPR and NVIC_ISER respectively.
 */
install_gpio_isr:
    ldr     r2, =(PPB_BASE + M0PLUS_VTOR_OFFSET)                @ Read vector table base address
    ldr     r1, [r2]                                            
    movs    r2, #GPIO_ISR_OFFSET                                
    add     r2, r1                                              @ Compute GPIO vector slot address
    ldr     r0, =gpio_isr                           
    str     r0, [r2]                                @ Write ISR address into vector table

    ldr     r2, =(PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET)           @ Clear pending GPIO interrupt (bit 13)
    ldr     r1, =8192           
    str     r1, [r2]            

    ldr     r2, =(PPB_BASE + M0PLUS_NVIC_ISER_OFFSET)           @ Enable GPIO interrupt in NVIC (bit 13)
    ldr     r1, =8192           
    str     r1, [r2]

    bx      lr                                                  

/**
 * @brief   GPIO interrupt service routine — decodes button press duration into dot or dash.
 *
 * Reads IO_BANK0_PROC0_INTS2 to determine which edge fired, then clears the
 * interrupt in IO_BANK0_INTR2.
 *
 * - **Rising edge (button pressed):** Records the current millisecond timestamp
 *   in @c press_start.
 * - **Falling edge (button released):** Computes the press duration via
 *   get_time_difference. Presses shorter than 50 ms are debounced and ignored.
 *   Valid presses reset @c timer_ticks (cancels any pending alarm tick),
 *   re-arm the alarm, clear @c press_start, then classify:
 *     - duration < @c DOT (255 ms) → writes 1 to @c isr_input (dot)
 *     - duration >= @c DOT        → writes 2 to @c isr_input (dash)
 *
 * @note    @c isr_input and @c timer_ticks are read by @ref user_input and
 *          @ref alrm_isr respectively.
 */
.thumb_func                                                     
gpio_isr:
    push    {r4-r7, lr}                                         @ Protect registers

    ldr     r2, =(IO_BANK0_BASE + IO_BANK0_PROC0_INTS2_OFFSET)  @ Read raw interrupt status
    ldr     r1, [r2]                                            
    ldr     r2, =(IO_BANK0_BASE + IO_BANK0_INTR2_OFFSET)
    ldr     r0, =(GPIO_BTN_RISE_MSK | GPIO_BTN_FALL_MSK)
    str     r0, [r2]                                            @ Acknowledge / clear both edge interrupts

    ldr     r2, =GPIO_BTN_RISE_MSK
    tst     r1, r2
    bne     rising_edge

    ldr     r2, =GPIO_BTN_FALL_MSK
    tst     r1, r2
    bne     falling_edge
    b       end_input

rising_edge:
    bl      get_time_in_ms
    ldr     r1, =press_start
    str     r0, [r1]                                            @ Record timestamp of button press
    b       end_input

falling_edge:
    bl      get_time_in_ms
    ldr     r1, =press_start
    ldr     r7, [r1]                                            @ Retrieve press-start timestamp
    movs    r1, r7
    bl      get_time_difference                                 @ r0 = duration in ms

    cmp     r0, #50
    blt     end_input                                           @ Ignore presses shorter than 50 ms (debounce)

    ldr     r1, =timer_ticks                                    @ Reset tick counter — new input resets spacing window
    movs    r2, #0
    str     r2, [r1]

    movs    r7, r0                                              @ Save duration before watchdog/alarm calls clobber r0
    bl      watchdog_update
    bl      set_alarm                                           @ Re-arm alarm from this moment

    ldr     r1, =press_start                                    @ Clear press_start to signal no press in progress
    movs    r2, #0
    str     r2, [r1]

    cmp     r7, #DOT                                            @ Classify as dot or dash
    blt     input_dot
    bge     input_dash
    b       end_input

input_dot:
    ldr     r1, =isr_input
    ldr     r2, =1                                              @ 1 = dot
    str     r2, [r1]                                            
    b       end_input

input_dash:
    ldr     r1, =isr_input
    ldr     r2, =2                                              @ 2 = dash
    str     r2, [r1]                                            

end_input:
    pop     {r4-r7, pc}                                         @ Restore registers


.data
    .align 4
print_dot:    .asciz ".\n"    @< String printed when a dot is decoded. 
print_dash:   .asciz "-\n"    @< String printed when a dash is decoded. 
print_space:  .asciz " \n"    @< String printed for an inter-word space. 
print_enter:  .asciz "n\n"    @< String printed on input submission.     
