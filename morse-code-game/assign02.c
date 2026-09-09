/**
 * @file    assign02.c
 * @brief   C support layer for the RP2040 Morse Code learning game.
 *
 * Provides GPIO helpers, WS2812 RGB LED control, PWM buzzer output, Morse code
 * lookup tables, level question display, answer checking, game-state management,
 * and the hardware entry point (@ref main) that bootstraps the assembly core
 * via @ref main_asm.
 *
 * @note    All interrupt-shared variables (@c isr_input, @c timer_ticks,
 *          @c press_start, @c reset_requested) are declared @c volatile so that
 *          the compiler does not cache them in registers across ISR boundaries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "ws2812.pio.h"
#include "hardware/watchdog.h"

/** @defgroup config Hardware Configuration
 *  Compile-time constants that describe the physical board wiring.
 *  @{
 */
#define IS_RGBW     false   /**< WS2812 colour order: RGB (not RGBW). */
#define NUM_PIXELS  1       /**< Number of WS2812 pixels on the chain. */
#define WS2812_PIN  28      /**< GPIO pin driving the WS2812 data line. */
#define BUZZER_PIN  18      /**< GPIO pin connected to the passive buzzer (PWM). */
/** @} */

/**
 * @brief   Full welcome/instruction banner printed to the serial console on startup.
 *
 * Contains game rules, button timing guidance, life/LED colour mapping, and the
 * SOS reset hint.
 */
#define WELCOME "************************************************************************************\n*									           *\n*		LEARN MORSE CODE - An interactive game on the RP2040               *\n*									           *\n************************************************************************************\n*										   *\n*	Welcome to our game! This code was written by Group 22 in CSU23021	   *\n*										   *\n*			Some instructions to get you started:			   *\n*										   *\n*1. Choose your level by entering the morse code for the level number (don't worry *\n* yet, these are provided for now.                                               *\n*2. Enter a (dot) by pressing the GP21 button on the MAKER-PI-PICO board for a     *\n* short duration (255ms or less), and enter a (dash) by pressing the GP21        *\n* button on the MAKER-PI-PICO board for a longer duration (longer than 255ms)    *\n*3. You will begin with 3 lives. Each time a correct answer is input, you will gain*\n* a life (with 3 lives maximum), and each time your answer is incorrect, a life  *\n* will be deducted. The RGB LED will indicate how many lives you have got left:  *\n* PURPLE: 3 lives                                    *\n* ORANGE: 2 lives                                    *\n* RED: 1 life                                        *\n* OFF: 0 lives, GAME OVER                            *\n*4. Type SOS (...---...) at any time during gameplay to reset to the welcome screen.*\n*										   *\n************************************************************************************\n"

/**
 * @brief   Entry point of the assembly portion of the program.
 *
 * Defined in main.S. Called from @ref main after all C-side hardware
 * initialisation is complete. Does not return under normal operation.
 */
void main_asm();

/* ============================================================
 * GPIO Initialisation
 * ============================================================ */

/**
 * @brief   Initialises a GPIO pin and enables its internal pull-up resistor.
 *
 * Thin wrapper around the Pico SDK so the assembly layer can call a
 * single symbol rather than two separate SDK functions.
 *
 * @param pin   GPIO pin number to initialise.
 */
void asm_gpio_init(uint pin)
{
    gpio_init(pin);
    gpio_pull_up(pin);
}

/**
 * @brief   Sets the direction of a GPIO pin.
 *
 * @param pin   GPIO pin number.
 * @param out   @c true to configure as output, @c false for input.
 */
void asm_gpio_set_dir(uint pin, bool out)
{
    gpio_set_dir(pin, out);
}

/**
 * @brief   Reads the current logic level of a GPIO input pin.
 *
 * @param pin   GPIO pin number to read.
 * @return      @c true if the pin is high, @c false if low.
 */
bool asm_gpio_get(uint pin)
{
    return gpio_get(pin);
}

/**
 * @brief   Drives a GPIO output pin to the specified logic level.
 *
 * @param pin   GPIO pin number to drive.
 * @param value @c true for high, @c false for low.
 */
void asm_gpio_put(uint pin, bool value)
{
    gpio_put(pin, value);
}

/**
 * @brief   Enables both-edge (rise and fall) interrupts on a GPIO pin.
 *
 * Also ensures the pull-up resistor is active so the pin is not floating
 * while no button is pressed.
 *
 * @param pin   GPIO pin number on which to enable edge interrupts.
 */
void asm_gpio_set_irq(uint pin)
{
    gpio_pull_up(pin);
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(pin, GPIO_IRQ_EDGE_RISE, true);
}

/**
 * @brief   No-operation GPIO IRQ callback used to register the IRQ handler slot.
 *
 * The RP2040 SDK requires a callback to be registered via
 * @c gpio_set_irq_enabled_with_callback before the assembly ISR can take
 * ownership of the vector. This stub satisfies that requirement without
 * performing any work.
 *
 * @param gpio   GPIO pin that triggered the interrupt (unused).
 * @param events Bitmask of events that fired (unused).
 */
static void dummy_gpio_irq(uint gpio, uint32_t events)
{
    (void)gpio;
    (void)events;
}

/* ============================================================
 * Watchdog Initialisation
 * ============================================================ */

/**
 * @brief   Prints a diagnostic message if the previous reset was caused by the watchdog.
 *
 * Should be called once during startup. Does not arm the watchdog — that is
 * deferred to @ref level_select_false so that the watchdog only runs during
 * active gameplay.
 */
void watchdog_init()
{
    if (watchdog_caused_reboot())
    {
        printf("\nNo input detected — rebooted by watchdog\n");
    }
    if (watchdog_enable_caused_reboot())
    {
        printf("\nChip reboot due to watchdog enable\n");
    }
}

/* ============================================================
 * RGB LED (WS2812)
 * ============================================================ */

/**
 * @brief   Pushes a single GRB-packed pixel value to the WS2812 state machine.
 *
 * The WS2812 protocol expects data MSB-first; the SDK PIO program requires the
 * colour to occupy the top 24 bits of the 32-bit word, hence the left-shift by 8.
 *
 * @param pixel_grb   Packed colour in GRB order (bits 23:16 = G, 15:8 = R, 7:0 = B).
 */
static inline void put_pixel(uint32_t pixel_grb)
{
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

/**
 * @brief   Packs individual R, G, B byte components into a single GRB word.
 *
 * The WS2812 expects green before red in its serial bitstream, so this helper
 * swaps R and G relative to the conventional RGB ordering.
 *
 * @param r   Red intensity   (0–255).
 * @param g   Green intensity (0–255).
 * @param b   Blue intensity  (0–255).
 * @return    Packed 32-bit GRB colour value suitable for @ref put_pixel.
 */
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)(r) << 8) |
           ((uint32_t)(g) << 16) |
           (uint32_t)(b);
}

/**
 * @brief   Updates the RGB LED colour to reflect the player's remaining lives.
 *
 * Colour mapping:
 * | Lives | Colour | Hex (R, G, B)        |
 * |-------|--------|----------------------|
 * | 3     | Purple | (0x4B, 0x00, 0x82)   |
 * | 2     | Orange | (0xFF, 0xA5, 0x00)   |
 * | 1     | Red    | (0x7F, 0x00, 0x00)   |
 * | 0     | Off    | (0x00, 0x00, 0x00)   |
 *
 * @param remaining_lives   Current life count (expected range: 0–3).
 */
void RGB_Lives_Display(int remaining_lives)
{
    if (remaining_lives == 0)
    {
        put_pixel(urgb_u32(0x00, 0x00, 0x00));
        printf("Game over!!!\n");
    }
    else if (remaining_lives == 1)
    {
        put_pixel(urgb_u32(0x7F, 0x00, 0x00));
        printf("Warning! 1 life remaining.\n");
    }
    else if (remaining_lives == 2)
    {
        put_pixel(urgb_u32(0xFF, 0xA5, 0x00));
        printf("2 lives remaining.\n");
    }
    else if (remaining_lives == 3)
    {
        put_pixel(urgb_u32(0x4B, 0x00, 0x82));
        printf("Congratulations! 3 lives remaining.\n");
    }
    else
    {
        printf("Throw error detection.\n");
    }
}

/* ============================================================
 * PWM Buzzer
 * ============================================================ */

/**
 * @brief   Configures the PWM slice driving the passive buzzer.
 *
 * Sets the PWM clock divider to 125, wrap value to 499, and duty cycle to
 * 50 % (level 250) to produce an audible ~2 kHz tone. The PWM output is left
 * disabled until @ref buzzer_beep is called.
 *
 * @note    Assumes the system clock is 125 MHz so that
 *          f_PWM = 125 MHz / 125 / 500 ≈ 2 kHz.
 */
void buzzer_init()
{
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);

    pwm_set_clkdiv(slice, 125.0f);
    pwm_set_wrap(slice, 499);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(BUZZER_PIN), 250);
    pwm_set_enabled(slice, false);
}

/**
 * @brief   Enables the buzzer for a fixed duration, then silences it.
 *
 * Used to give audible feedback for dot (150 ms) and dash (450 ms) inputs.
 * Blocks the calling context for @p duration_ms milliseconds.
 *
 * @param duration_ms   Tone length in milliseconds.
 */
void buzzer_beep(int duration_ms)
{
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice, true);
    sleep_ms(duration_ms);
    pwm_set_enabled(slice, false);
}

/* ============================================================
 * Button Press Timing
 * ============================================================ */

/**
 * @brief   Returns the number of milliseconds elapsed since boot.
 *
 * Wraps the Pico SDK @c get_absolute_time / @c to_ms_since_boot pair into a
 * simple @c int return so the assembly ISR can call it directly via BL.
 *
 * @return  Current timestamp in milliseconds (wraps after ~49 days).
 */
int get_time_in_ms()
{
    absolute_time_t time = get_absolute_time();
    return to_ms_since_boot(time);
}

/**
 * @brief   Computes the elapsed time between two millisecond timestamps.
 *
 * @param end_time    Timestamp recorded at button release (ms since boot).
 * @param start_time  Timestamp recorded at button press   (ms since boot).
 * @return            Duration of the press in milliseconds.
 */
int get_time_difference(int end_time, int start_time)
{
    return (end_time - start_time);
}

/* ============================================================
 * Global Variables
 * ============================================================ */

/** @defgroup game_state Game State Variables
 *  Variables that track the player's progress during a session.
 *  @{
 */
int Remaining_Lives  = 3;   /**< Lives the player currently has (0–3). */
int Gained_Lives     = 0;   /**< Cumulative lives gained this level (for stats). */
int Correct_Counter  = 0;   /**< Correct answers given in the current level. */
int levelIndex       = 0;   /**< Active level index (1–4; 5 signals invalid/won). */
char *userInput;            /**< Pointer reserved for future use. */
int userInputLength;        /**< Length of @c userInput (reserved). */
int lastInput;              /**< Most recent raw input code (reserved). */

char inputSequence[32];     /**< Accumulation buffer for the current Morse sequence. */
int i               = 0;    /**< Write cursor into @c inputSequence. */
int tmpIndex        = 0;    /**< Index into @c alp[] / @c que[] for the current question. */
int inputComplete   = 0;    /**< Set to 1 when the player submits (2-second gap). */
int Remain_Counter  = 5;    /**< Questions remaining in the current level (counts down from 5). */
int Wrong_Counter   = 0;    /**< Incorrect answers given in the current level (for stats). */

int select_level_input = 0; /**< Non-zero while the player is choosing a level. */
/** @} */

/** @defgroup isr_shared ISR-Shared Variables
 *  Written by ISRs in main.S and read by the main C/ASM loop. All are
 *  @c volatile to prevent the compiler from caching them in registers.
 *  @{
 */
volatile int isr_input    = 3; /**< Last decoded input: 1=dot, 2=dash, 3=none (sentinel). */
volatile int timer_ticks  = 0; /**< Alarm tick count since last button event (1=gap, 2=enter). */
volatile int press_start  = 0; /**< Millisecond timestamp of the most recent button press,
                                 *   or 0 if no press is in progress. */
volatile int reset_requested = 0; /**< Set to 1 by @ref reset_to_welcome to signal the ASM
                                    *  loop to jump to @c restart_game. */
/** @} */

/**
 * @brief   Returns the current value of the SOS reset flag.
 *
 * Called from the assembly layer which cannot read a C @c volatile directly
 * in a conditional branch without an intervening BL.
 *
 * @return  1 if an SOS reset has been requested, 0 otherwise.
 */
int is_reset_requested()
{
    return reset_requested;
}

/**
 * @brief   Clears the SOS reset flag after the assembly loop has handled it.
 */
void clear_reset_request()
{
    reset_requested = 0;
}

/* ============================================================
 * Morse Code Table
 * ============================================================ */

/** @brief Number of entries in the Morse code lookup table (26 letters + 10 digits). */
#define table_SIZE 36

/**
 * @brief   Associates a printable character with its Morse code string.
 */
typedef struct morsecode
{
    char  letter; /**< The alphanumeric character (A–Z, 0–9). */
    char *code;   /**< Null-terminated Morse representation using '.' and '-'. */
} morsecode;

/** @brief Global Morse code lookup table, populated by @ref morse_init. */
morsecode table[table_SIZE];

/**
 * @brief   Populates the global @ref table with letters, digits, and their Morse codes.
 *
 * Must be called once before any Morse encoding/decoding takes place. Letters
 * A–Z occupy indices 0–25; digits 0–9 occupy indices 26–35. Code strings are
 * compile-time string literals stored in flash.
 */
void morse_init()
{
    table[0].letter = 'A';
    table[1].letter = 'B';
    table[2].letter = 'C';
    table[3].letter = 'D';
    table[4].letter = 'E';
    table[5].letter = 'F';
    table[6].letter = 'G';
    table[7].letter = 'H';
    table[8].letter = 'I';
    table[9].letter = 'J';
    table[10].letter = 'K';
    table[11].letter = 'L';
    table[12].letter = 'M';
    table[13].letter = 'N';
    table[14].letter = 'O';
    table[15].letter = 'P';
    table[16].letter = 'Q';
    table[17].letter = 'R';
    table[18].letter = 'S';
    table[19].letter = 'T';
    table[20].letter = 'U';
    table[21].letter = 'V';
    table[22].letter = 'W';
    table[23].letter = 'X';
    table[24].letter = 'Y';
    table[25].letter = 'Z';
    table[26].letter = '0';
    table[27].letter = '1';
    table[28].letter = '2';
    table[29].letter = '3';
    table[30].letter = '4';
    table[31].letter = '5';
    table[32].letter = '6';
    table[33].letter = '7';
    table[34].letter = '8';
    table[35].letter = '9';

    table[0].code  = ".-";
    table[1].code  = "-...";
    table[2].code  = "-.-.";
    table[3].code  = "-..";
    table[4].code  = ".";
    table[5].code  = "..-.";
    table[6].code  = "--.";
    table[7].code  = "....";
    table[8].code  = "..";
    table[9].code  = ".---";
    table[10].code = "-.-";
    table[11].code = ".-..";
    table[12].code = "--";
    table[13].code = "-.";
    table[14].code = "---";
    table[15].code = ".--.";
    table[16].code = "--.-";
    table[17].code = ".-.";
    table[18].code = "...";
    table[19].code = "-";
    table[20].code = "..-";
    table[21].code = "...-";
    table[22].code = ".--";
    table[23].code = "-..-";
    table[24].code = "-.--";
    table[25].code = "--..";
    table[26].code = "-----";
    table[27].code = ".----";
    table[28].code = "..---";
    table[29].code = "...--";
    table[30].code = "....-";
    table[31].code = ".....";
    table[32].code = "-....";
    table[33].code = "--...";
    table[34].code = "---..";
    table[35].code = "----.";
}

/**
 * @brief   Flat Morse code string array indexed identically to @ref que.
 *
 * Indices 0–25 = letters A–Z, indices 26–35 = digits 0–9. Used by level
 * question functions and answer checking to avoid re-indexing through
 * @ref table.
 */
static const char *alp[] = {
    ".-",    "-...",  "-.-.",  "-..",   ".",     "..-.",
    "--.",   "....",  "..",    ".---",  "-.-",   ".-..",
    "--",    "-.",    "---",   ".--.",  "--.-",  ".-.",
    "...",   "-",     "..-",   "...-",  ".--",   "-..-",
    "-.--",  "--..",  "-----", ".----", "..---", "...--",
    "....-", ".....", "-....", "--...", "---..", "----.",
};

/**
 * @brief   Printable character strings parallel to @ref alp.
 *
 * @c que[i] is the human-readable label for the Morse code @c alp[i].
 * Used in question display to show what the player should encode.
 */
static const char *que[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/* ============================================================
 * Helper: strip spaces
 * ============================================================ */

/**
 * @brief   Copies @p src into @p dst omitting all space characters.
 *
 * Used for lenient answer matching: because a 1-second alarm tick generates a
 * space in @c inputSequence, stripping spaces before @c strcmp means a brief
 * mid-character pause does not cause an incorrect verdict.
 *
 * @param src       Null-terminated source string (may contain spaces).
 * @param dst       Output buffer that receives the space-free copy.
 * @param dst_size  Size of @p dst in bytes; output is always null-terminated.
 */
static void strip_spaces(const char *src, char *dst, int dst_size)
{
    int j = 0;
    for (int k = 0; src[k] != '\0' && j < dst_size - 1; k++)
    {
        if (src[k] != ' ')
        {
            dst[j++] = src[k];
        }
    }
    dst[j] = '\0';
}

/* ============================================================
 * Starting Game
 * ============================================================ */

/**
 * @brief   Resets all game-state counters and sets the RGB LED to green.
 *
 * Called at the start of each game session and after every level transition via
 * the assembly @c gameStart label. Resets @ref Correct_Counter,
 * @ref Remaining_Lives, @ref Remain_Counter, @ref Wrong_Counter, and
 * @ref Gained_Lives to their initial values and feeds the watchdog.
 */
void gameStart()
{
    put_pixel(urgb_u32(0x00, 0x7F, 0x00));
    Correct_Counter  = 0;
    Remaining_Lives  = 3;
    Remain_Counter   = 5;
    Wrong_Counter    = 0;
    Gained_Lives     = 0;
    watchdog_update();
}

/* ============================================================
 * Level Selection
 * ============================================================ */

/**
 * @brief   Signals that the player is currently in the level-selection phase.
 *
 * Sets @ref select_level_input to 1 so that @ref detectInput routes a
 * completed sequence to @ref select_level instead of @ref display_user_input.
 */
void level_select_true()
{
    select_level_input = 1;
}

/**
 * @brief   Signals that level selection is complete and enables the watchdog.
 *
 * Clears @ref select_level_input, then arms the hardware watchdog with the
 * maximum timeout (~8 seconds). From this point the main loop must call
 * @c watchdog_update periodically or the chip will reboot.
 */
void level_select_false()
{
    select_level_input = 0;
    watchdog_enable(0x7fffff, 1);
    watchdog_update();
}

/**
 * @brief   Decodes the contents of @c inputSequence as a level number.
 *
 * Strips spaces from @c inputSequence and compares the result against the
 * Morse codes for digits 1–4. On a match, @ref levelIndex is set and the
 * matched level number (1–4) is returned. Any other sequence is treated as
 * invalid: @ref levelIndex is set to 5 and 5 is returned.
 *
 * @return  Selected level (1–4) or 5 on invalid input.
 */
int select_level()
{
    char stripped[32];
    strip_spaces(inputSequence, stripped, sizeof(stripped));

    printf("\nChecking level selection, input='%s'\n", stripped);
    if (strcmp(stripped, alp[27]) == 0)
    {
        printf("Level 1 selected!\n");
        levelIndex = 1;
        return 1;
    }
    else if (strcmp(stripped, alp[28]) == 0)
    {
        printf("Level 2 selected!\n");
        levelIndex = 2;
        return 2;
    }
    else if (strcmp(stripped, alp[29]) == 0)
    {
        printf("Level 3 selected!\n");
        levelIndex = 3;
        return 3;
    }
    else if (strcmp(stripped, alp[30]) == 0)
    {
        printf("Level 4 selected!\n");
        levelIndex = 4;
        return 4;
    }
    else
    {
        printf("Invalid input, try again!\n");
        levelIndex = 5;
        return 5;
    }
}

/**
 * @brief   Returns a uniformly distributed random integer in [@p low, @p high].
 *
 * If @p low > @p high the function clamps and returns @p high.
 *
 * @param low   Inclusive lower bound.
 * @param high  Inclusive upper bound.
 * @return      Random integer in the specified range.
 */
int r(int low, int high)
{
    if (low > high)
        return high;
    return low + (rand() % (high - low + 1));
}

/* ============================================================
 * Level Question Display
 * ============================================================ */

/**
 * @brief   Selects and displays a random Level 1 question.
 *
 * Level 1 shows both the Morse code sequence and the corresponding character,
 * so the player can practise recognition before encoding. Sets @ref levelIndex
 * to 1 and stores the chosen character index in @ref tmpIndex.
 *
 * @return  Index into @ref alp / @ref que for the chosen character (0–35).
 */
int level_1_question()
{
    levelIndex = 1;
    int tmp = r(0, 35);
    tmpIndex = tmp;
    printf("\n");
    printf("||================================================||\n");
    printf("||     Reproduce this character in morse code     ||\n");
    printf("                          %s                        \n", alp[tmp]);
    printf("                          %s                        \n", que[tmp]);
    printf("||================================================||\n");
    return tmp;
}

/**
 * @brief   Selects and displays a random Level 2 question.
 *
 * Level 2 shows only the printable character; the player must recall the
 * Morse encoding without being given the dot/dash sequence. Sets @ref levelIndex
 * to 2 and stores the chosen character index in @ref tmpIndex.
 *
 * @return  Index into @ref alp / @ref que for the chosen character (0–35).
 */
int level_2_question()
{
    levelIndex = 2;
    int tmp = r(0, 35);
    tmpIndex = tmp;
    printf("\n");
    printf("||================================================||\n");
    printf("||           What is this in morse code?          ||\n");
    printf("                         %c                         \n", table[tmp].letter);
    printf("||================================================||\n");
    return tmp;
}

/**
 * @brief   Persistent question index for Level 3 (cycles 0–4 across the five words).
 */
int level3QuestionIdx = 0;

/**
 * @brief   Displays the next Level 3 word question in a fixed rotation.
 *
 * Level 3 presents five multi-character words (PICO, CODE, MICRO, INTEL, GROUP)
 * in order, displaying the Morse code hint. The rotation index @c level3QuestionIdx
 * advances after each call and wraps back to 0 after the fifth word.
 *
 * @c tmpIndex is set to 36–40 to identify which word answer is expected:
 * - 36 = PICO, 37 = CODE, 38 = MICRO, 39 = INTEL, 40 = GROUP.
 *
 * @return  The @c tmpIndex value assigned (36–40), or 0 on an unexpected state.
 */
int level_3_question()
{
    levelIndex = 3;
    int toReturn = 0;
    if (level3QuestionIdx == 0)
    {
        toReturn = 36;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"PICO\" in morse code\n");
        printf(".--. .. -.-. ---\n");
        printf("================================================\n");
        level3QuestionIdx = 1;
        return toReturn;
    }
    if (level3QuestionIdx == 1)
    {
        toReturn = 37;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"CODE\" in morse code\n");
        printf("-.-. --- -.. .\n");
        printf("================================================\n");
        level3QuestionIdx = 2;
        return toReturn;
    }
    if (level3QuestionIdx == 2)
    {
        toReturn = 38;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"MICRO\" in morse code\n");
        printf("-- .. -.-. .-. ---\n");
        printf("================================================\n");
        level3QuestionIdx = 3;
        return toReturn;
    }
    if (level3QuestionIdx == 3)
    {
        toReturn = 39;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"INTEL\" in morse code\n");
        printf(".. -. - . .-..\n");
        printf("================================================\n");
        level3QuestionIdx = 4;
        return toReturn;
    }
    if (level3QuestionIdx == 4)
    {
        toReturn = 40;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"GROUP\" in morse code\n");
        printf("--. .-. --- ..- .--.\n");
        printf("================================================\n");
        level3QuestionIdx = 0;
        return toReturn;
    }
    return toReturn;
}

/**
 * @brief   Persistent question index for Level 4 (cycles 0–4 across the five words).
 */
int level4QuestionIdx = 0;

/**
 * @brief   Displays the next Level 4 word question in a fixed rotation.
 *
 * Identical word set to Level 3 (PICO, CODE, MICRO, INTEL, GROUP) but without
 * the Morse code hint, requiring the player to encode each word from memory.
 * @c tmpIndex values 36–40 are used identically to @ref level_3_question.
 *
 * @return  The @c tmpIndex value assigned (36–40), or 0 on an unexpected state.
 */
int level_4_question()
{
    levelIndex = 4;
    int toReturn = 0;
    if (level4QuestionIdx == 0)
    {
        toReturn = 36;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"PICO\" in morse code\n");
        printf("================================================\n");
        level4QuestionIdx = 1;
        return toReturn;
    }
    if (level4QuestionIdx == 1)
    {
        toReturn = 37;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"CODE\" in morse code\n");
        printf("================================================\n");
        level4QuestionIdx = 2;
        return toReturn;
    }
    if (level4QuestionIdx == 2)
    {
        toReturn = 38;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"MICRO\" in morse code\n");
        printf("================================================\n");
        level4QuestionIdx = 3;
        return toReturn;
    }
    if (level4QuestionIdx == 3)
    {
        toReturn = 39;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"INTEL\" in morse code\n");
        printf("================================================\n");
        level4QuestionIdx = 4;
        return toReturn;
    }
    if (level4QuestionIdx == 4)
    {
        toReturn = 40;
        tmpIndex = toReturn;
        printf("\n================================================\n");
        printf("Reproduce the word \"GROUP\" in morse code\n");
        printf("================================================\n");
        level4QuestionIdx = 0;
        return toReturn;
    }
    return toReturn;
}

/**
 * @brief   Returns the currently active level index.
 *
 * Called from the assembly layer to decide which question routine to invoke.
 *
 * @return  @ref levelIndex (1–5; 5 means invalid/all levels complete).
 */
int get_level()
{
    return levelIndex;
}

/**
 * @brief   Overrides the currently active level index.
 *
 * Called from the assembly layer after a level is completed to advance to
 * the next level, or after level selection to set the starting level.
 *
 * @param level   New level index to store (1–5).
 */
void set_level(int level)
{
    levelIndex = level;
}

/**
 * @brief   Determines whether the current level or game session has ended.
 *
 * Checks (in priority order):
 * 1. SOS reset flag — returns 2 and clears the flag.
 * 2. Five correct answers — returns 1 (level complete).
 * 3. Zero remaining lives — prints a game-over summary and returns 2 (restart).
 *
 * @return  0 = continue playing, 1 = level complete, 2 = restart game.
 */
int check_level_complete()
{
    if (reset_requested)
    {
        reset_requested = 0;
        return 2;
    }

    if (Correct_Counter == 5)
    {
        return 1;
    }
    if (Remaining_Lives == 0)
    {
        printf("\n");
        printf("||=================================||\n");
        printf("||                                 ||\n");
        printf("||      No Lives remaining         ||\n");
        printf("||  Sorry, you've lost the game    ||\n");
        printf("||                                 ||\n");
        printf("||=================================||\n");
        printf("\n");
        printf("||========================================||\n");
        printf("||             Statistics Table           ||\n");
        printf("||             Correct times: %d          ||\n", Correct_Counter);
        printf("||             Wrong times: %d            ||\n", Wrong_Counter);
        printf("||             Gained Lives: %d           ||\n", Gained_Lives);
        printf("||             Remaining Lives: %d        ||\n", Remaining_Lives);
        printf("||========================================||\n");
        printf("\n");
        return 2;
    }
    return 0;
}

/* ============================================================
 * Reading Inputs
 * ============================================================ */

/**
 * @brief   Clears @c inputSequence and resets the write cursor and completion flag.
 *
 * Must be called before each new question so that leftover dots, dashes, and
 * spaces from the previous answer do not contaminate the next comparison.
 */
void initalizeInputArray()
{
    int maxsize = 32;
    for (int j = 0; j < maxsize; j++)
    {
        inputSequence[j] = '\0';
    }
    inputComplete = 0;
    i = 0;
}

/* Forward declarations for functions referenced before their definitions. */
void display_user_input();
void Display_Welcome_Messages();

/**
 * @brief   Handles detection of the SOS reset sequence during gameplay.
 *
 * Prints a reset notification to the console, pulses the LED blue, redisplays
 * the welcome screen, and sets @ref reset_requested so the assembly loop jumps
 * to @c restart_game on its next @c check_level_complete call.
 */
void reset_to_welcome()
{
    printf("\n");
    printf("||=================================||\n");
    printf("||   SOS detected — resetting...   ||\n");
    printf("||=================================||\n");
    printf("\n");

    put_pixel(urgb_u32(0x00, 0x00, 0x7F));
    Display_Welcome_Messages();
    reset_requested = 1;
}

/**
 * @brief   Tests whether the current @c inputSequence (spaces stripped) is SOS.
 *
 * Strips inter-letter spaces so a player who naturally pauses between the
 * S, O, and S groups still triggers the reset.
 *
 * @return  1 if the stripped sequence equals @c "...---...", 0 otherwise.
 */
static int is_sos_sequence(void)
{
    char stripped[32];
    strip_spaces(inputSequence, stripped, sizeof(stripped));
    return strcmp(stripped, "...---...") == 0;
}

/**
 * @brief   Appends a decoded Morse symbol to @c inputSequence and handles submission.
 *
 * Called by the assembly layer's @c user_input routine each time a dot, dash,
 * space, or enter event is decoded from the ISR shared variables.
 *
 * | @p input | Meaning          | Action                                              |
 * |----------|------------------|-----------------------------------------------------|
 * | 1        | Dot              | Appends '.', beeps 150 ms, checks for live SOS.     |
 * | 2        | Dash             | Appends '-', beeps 450 ms, checks for live SOS.     |
 * | 3        | Inter-char space | Appends ' ' (unless already trailing space).        |
 * | 4        | Submit (enter)   | Strips trailing spaces, sets @c inputComplete, then |
 * |          |                  | routes to @ref select_level or @ref display_user_input.|
 *
 * Buffer overflow protection: input is silently ignored when @c i >= 30.
 *
 * @param input   Symbol code (1 = dot, 2 = dash, 3 = space, 4 = enter).
 */
void detectInput(int input)
{
    if (i >= 30)
        return;

    if (input == 1)
    {
        inputSequence[i] = '.';
        i++;
        inputSequence[i] = '\0';
        printf(".");
        buzzer_beep(10);

        if (is_sos_sequence())
        {
            reset_to_welcome();
            return;
        }
    }
    else if (input == 2)
    {
        inputSequence[i] = '-';
        i++;
        inputSequence[i] = '\0';
        printf("-");
        buzzer_beep(100);

        if (is_sos_sequence())
        {
            reset_to_welcome();
            return;
        }
    }
    else if (input == 3)
    {
        if (i == 0 || inputSequence[i - 1] == ' ')
            return;

        inputSequence[i] = ' ';
        i++;
        inputSequence[i] = '\0';

        if (select_level_input == 1)
        {
            printf("\n  (gap detected - wait 1 more second to submit)\n");
        }
    }
    else if (input == 4)
    {
        while (i > 0 && inputSequence[i - 1] == ' ')
        {
            inputSequence[i - 1] = '\0';
            i--;
        }

        inputComplete = 1;

        if (select_level_input == 1)
        {
            printf("\n[enter triggered, seq='%s']\n", inputSequence);
            select_level();
        }
        else
        {
            if (is_sos_sequence())
            {
                reset_to_welcome();
                return;
            }

            display_user_input();
            RGB_Lives_Display(Remaining_Lives);
        }
    }
}

/* ============================================================
 * Checking Answer
 * ============================================================ */

/**
 * @brief   Checks whether the player's input matches the expected Morse code.
 *
 * Strips spaces from @c inputSequence and compares it against @c alp[tmpIndex].
 * Lenient matching means a single stray 1-second alarm tick mid-character does
 * not cause an incorrect verdict.
 *
 * @return  1 if the stripped input matches the expected code, 0 otherwise.
 */
int checkAnswer()
{
    char stripped[32];
    strip_spaces(inputSequence, stripped, sizeof(stripped));

    if (strcmp(stripped, alp[tmpIndex]) == 0)
    {
        return 1;
    }
    return 0;
}

/**
 * @brief   Looks up the character whose Morse code matches the current input.
 *
 * Strips spaces from @c inputSequence, then performs a linear search through
 * @ref table. Used for levels where the player must identify a character from
 * its Morse code.
 *
 * @return  Index into @ref table of the matching entry, or -1 if not found.
 */
int checkMorseCode()
{
    char stripped[32];
    strip_spaces(inputSequence, stripped, sizeof(stripped));

    for (int k = 0; k < table_SIZE; k++)
    {
        if (strcmp(stripped, table[k].code) == 0)
        {
            return k;
        }
    }
    return -1;
}

/* ============================================================
 * Printing Game Messages
 * ============================================================ */

/**
 * @brief   Prints the full @ref WELCOME banner to the serial console.
 */
void Display_Welcome_Messages()
{
    printf(WELCOME);
}

/**
 * @brief   Returns whether the player still has lives remaining.
 *
 * @return  @c true if @ref Remaining_Lives > 0, @c false if the game is over.
 */
bool game_continue()
{
    if (Remaining_Lives <= 0)
    {
        return 0;
    }
    return 1;
}

/**
 * @brief   Prints a level-completion summary including the current statistics.
 *
 * Displays correct count (fixed at 5), wrong count, lives gained, and lives
 * remaining for the level that was just finished.
 */
void level_success()
{
    printf("\n");
    printf("========================================\n");
    printf("Complete current level %d!\n", levelIndex);
    printf("========================================\n");
    printf("\n");
    printf("||========================================||\n");
    printf("||             Statistics Table           ||\n");
    printf("||             Correct times: 5           ||\n");
    printf("||             Wrong times: %d            ||\n", Wrong_Counter);
    printf("||             Gained Lives: %d           ||\n", Gained_Lives);
    printf("||             Remaining Lives: %d        ||\n", Remaining_Lives);
    printf("||========================================||\n");
    printf("\n");
}

/**
 * @brief   Prints the game-won congratulations banner.
 */
void game_won_display()
{
    printf("||=======================================||\n");
    printf("||                                       ||\n");
    printf("||   Congurtulations! You won the game!  ||\n");
    printf("||                                       ||\n");
    printf("||=======================================||\n");
}

/**
 * @brief   Prints the game-over condolences banner.
 */
void game_over_display()
{
    printf("||=================================||\n");
    printf("||                                 ||\n");
    printf("||   Sorry, you lost the game!     ||\n");
    printf("||                                 ||\n");
    printf("||=================================||\n");
}

/**
 * @brief   Prints the level-selection prompt listing Morse codes for levels 1–4.
 */
void display_level_choosing()
{
    printf("\n");
    printf("You can choose the corresponding level according to the difficulty\n");
    printf("For level 1, type in corresponding morse code for number 1(.----)\n");
    printf("For level 2, type in corresponding morse code for number 2(..---)\n");
    printf("For level 3, type in corresponding morse code for number 3(...--)\n");
    printf("For level 4, type in corresponding morse code for number 4(....-)\n");
}

/**
 * @brief   Prints an invalid level-selection error banner.
 */
void level_selection_invalid()
{
    printf("||=======================================||\n");
    printf("||                                       ||\n");
    printf("||    Invalid Input, please try again    ||\n");
    printf("||                                       ||\n");
    printf("||=======================================||\n");
}

/**
 * @brief   Handles an incorrect answer for Levels 1 and 2 (single-character questions).
 *
 * Prints the player's input alongside the correct Morse code and character,
 * decrements @ref Remaining_Lives, and increments @ref Wrong_Counter.
 */
void incorrectInputlvl1()
{
    printf("\n\n");
    printf("||================================================||\n");
    printf("||                You entered:                    ||\n");
    printf("                     %s                             \n", inputSequence);
    printf("                   %s  =  %s                        \n", alp[tmpIndex], que[tmpIndex]);
    printf("||       this did not match any sequence          ||\n");
    printf("||================================================||\n");
    Remaining_Lives--;
    printf("\n");
    printf("||=================================||\n");
    printf("||     Wrong! One life was token.  ||\n");
    printf("||=================================||\n");
    Wrong_Counter++;
}

/**
 * @brief   Handles an incorrect answer for Levels 3 and 4 (multi-character word questions).
 *
 * Prints the player's input without revealing the correct answer (no Morse hint),
 * decrements @ref Remaining_Lives, and increments @ref Wrong_Counter.
 */
void incorrectInputlvl3()
{
    printf("\n\n");
    printf("||================================================||\n");
    printf("||                You entered:                    ||\n");
    printf("                     %s                             \n", inputSequence);
    printf("||       this did not match any sequence          ||\n");
    printf("||================================================||\n");
    Remaining_Lives--;
    printf("\n");
    printf("||=================================||\n");
    printf("||     Wrong! One life was token.  ||\n");
    printf("||=================================||\n");
    Wrong_Counter++;
}

/**
 * @brief   Handles a correct answer for Levels 1 and 2 (single-character questions).
 *
 * Increments @ref Correct_Counter and decrements @ref Remain_Counter, then
 * prints the matched Morse/character pair and progress. Awards a life (up to
 * the maximum of 3) and increments @ref Gained_Lives if a life is added.
 */
void correctInputlvl1()
{
    printf("\n\n");
    Correct_Counter++;
    Remain_Counter--;
    printf("||================================================||\n");
    printf("||                You entered:                    ||\n");
    printf("                   %s  =  %s                        \n", alp[tmpIndex], que[tmpIndex]);
    printf("   You have correct %d time(s),  %d time(s) left    \n", Correct_Counter, Remain_Counter);
    printf("||================================================||\n");
    if (Remaining_Lives < 3)
    {
        Remaining_Lives++;
        printf("\n");
        printf("||=================================||\n");
        printf("||   Correct! One life was added.  ||\n");
        printf("||=================================||\n");
        Gained_Lives++;
    }
    else
    {
        printf("\n");
        printf("||=================================||\n");
        printf("||            Correct!             ||\n");
        printf("||=================================||\n");
    }
}

/**
 * @brief   Handles a correct answer for Levels 3 and 4 (multi-character word questions).
 *
 * Increments @ref Correct_Counter and decrements @ref Remain_Counter, then
 * prints the matched sequence and progress. Awards a life (up to the maximum
 * of 3) and increments @ref Gained_Lives if a life is added.
 */
void correctInputlvl3()
{
    printf("\n\n");
    Correct_Counter++;
    Remain_Counter--;
    printf("||================================================||\n");
    printf("||                You entered:                    ||\n");
    printf("                   %s                         \n", inputSequence);
    printf("   You have correct %d time(s),  %d time(s) left    \n", Correct_Counter, Remain_Counter);
    printf("||================================================||\n");

    if (Remaining_Lives < 3)
    {
        Remaining_Lives++;
        printf("\n");
        printf("||=================================||\n");
        printf("||   Correct! One life was added.  ||\n");
        printf("||=================================||\n");
        Gained_Lives++;
    }
    else
    {
        printf("\n");
        printf("||=================================||\n");
        printf("||            Correct!             ||\n");
        printf("||=================================||\n");
    }
}

/**
 * @brief   Compares the current @c inputSequence (spaces stripped) against an expected
 *          word Morse string (also stripped).
 *
 * Used by @ref display_user_input to evaluate Level 3/4 word answers leniently,
 * so natural inter-letter pauses do not cause a wrong verdict.
 *
 * @param expected_with_spaces  Morse code string for the target word with
 *                              spaces between letter codes
 *                              (e.g. @c ".--. .. -.-. ---" for "PICO").
 * @return  1 if the stripped inputs match, 0 otherwise.
 */
static int match_word(const char *expected_with_spaces)
{
    char stripped_input[32];
    char stripped_expected[32];
    strip_spaces(inputSequence, stripped_input, sizeof(stripped_input));
    strip_spaces(expected_with_spaces, stripped_expected, sizeof(stripped_expected));
    return strcmp(stripped_input, stripped_expected) == 0;
}

/**
 * @brief   Evaluates the player's completed input and dispatches to the
 *          appropriate correct/incorrect feedback function.
 *
 * Dispatches based on @ref tmpIndex:
 * - 0–35: single character (Levels 1/2) — calls @ref checkAnswer, then
 *   @ref correctInputlvl1 or @ref incorrectInputlvl1.
 * - 36 (PICO), 37 (CODE), 38 (MICRO), 39 (INTEL), 40 (GROUP): word questions
 *   (Levels 3/4) — calls @ref match_word, then @ref correctInputlvl3 or
 *   @ref incorrectInputlvl3.
 */
void display_user_input()
{
    if (tmpIndex >= 0 && tmpIndex <= 35)
    {
        int temp = checkAnswer();
        if (inputComplete == 1)
        {
            if (temp == 0)
            {
                incorrectInputlvl1();
            }
            else
            {
                correctInputlvl1();
            }
        }
    }
    else if (tmpIndex == 36)
    {
        if (match_word(".--. .. -.-. ---"))
            correctInputlvl3();
        else
            incorrectInputlvl3();
    }
    else if (tmpIndex == 37)
    {
        if (match_word("-.-. --- -.. ."))
            correctInputlvl3();
        else
            incorrectInputlvl3();
    }
    else if (tmpIndex == 38)
    {
        if (match_word("-- .. -.-. .-. ---"))
            correctInputlvl3();
        else
            incorrectInputlvl3();
    }
    else if (tmpIndex == 39)
    {
        if (match_word(".. -. - . .-.."))
            correctInputlvl3();
        else
            incorrectInputlvl3();
    }
    else if (tmpIndex == 40)
    {
        if (match_word("--. .-. --- ..- .--."))
            correctInputlvl3();
        else
            incorrectInputlvl3();
    }
}

/* ============================================================
 * Main Function
 * ============================================================ */

/**
 * @brief   Hardware entry point — initialises all peripherals and transfers
 *          control to the assembly game loop.
 *
 * Execution order:
 * 1. @c stdio_init_all — USB/UART console.
 * 2. 2-second delay to allow the host terminal to connect.
 * 3. @ref morse_init — populate the Morse lookup table.
 * 4. @ref watchdog_init — log any watchdog-caused reboot.
 * 5. Seed the PRNG with the hardware timer for varied question ordering.
 * 6. Initialise PIO0 with the WS2812 program on @c WS2812_PIN.
 * 7. Set the LED to blue to indicate startup.
 * 8. @ref buzzer_init — configure the PWM buzzer.
 * 9. Print the welcome banner.
 * 10. Register the dummy GPIO IRQ callback so the assembly ISR can override it.
 * 11. Feed the watchdog, then call @ref main_asm (does not return).
 *
 * @return  0 (unreachable under normal operation).
 */
int main()
{
    stdio_init_all();
    sleep_ms(2000);
    morse_init();
    watchdog_init();

    srand(to_ms_since_boot(get_absolute_time()));

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, 0, offset, WS2812_PIN, 800000, IS_RGBW);
    put_pixel(urgb_u32(0x00, 0x00, 0x7F));

    buzzer_init();

    printf(WELCOME);

    gpio_set_irq_enabled_with_callback(21,
                                       GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                       false,
                                       (gpio_irq_callback_t)dummy_gpio_irq);

    watchdog_update();
    main_asm();

    return 0;
}