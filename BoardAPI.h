#ifndef LEDCONTROL_H
#define LEDCONTROL_H

#include "ti_msp_dl_config.h"
#include "stdint.h"

// ─────────────────────────────────────────────
// PWM Configuration
// ─────────────────────────────────────────────

typedef struct {
    GPTIMER_Regs *TIMER;
    uint8_t CC_INDEX;
    uint8_t is_complementary_output;
} PWM_Config;


// ─────────────────────────────────────────────
// LED Control State
// ─────────────────────────────────────────────

typedef struct {
    float set_current;   // currently set current in mA
} led_channel_t;

extern led_channel_t led_channel;       // single LED channel state
extern uint16_t global_led_voltage;     // currently set boost voltage in mV

// ─────────────────────────────────────────────
// Limits
// ─────────────────────────────────────────────

#define LED_HW_MAX_CURRENT_MA   2000     // hardware maximum current in mA
#define MAX_DUTY_CYCLES_LED     99      // number of entries in current LUT
#define MAX_DUTY_CYCLES_BOOST   35      // number of entries in voltage LUT

// ─────────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────────

/*
 * @brief Initialises PWM hardware and LED control state.
 *        Call once at startup before any other LED function.
 */
void LED_control_init(void);

// ─────────────────────────────────────────────
// Voltage Control
// ─────────────────────────────────────────────

/*
 * @brief Sets the LED boost converter output voltage.
 *        Can be called at any time to change the voltage.
 * @param voltage  Target voltage in mV (valid range ~3500 to ~11500)
 */
void LED_set_voltage(uint16_t voltage);

/*
 * @brief Returns the last voltage set via LED_set_voltage().
 * @return Voltage in mV
 */
uint16_t LED_get_voltage(void);

// ─────────────────────────────────────────────
// Current Control
// ─────────────────────────────────────────────

/*
 * @brief Sets the LED output current.
 *        Current is clamped to LED_HW_MAX_CURRENT_MA.
 * @param current  Target current in mA
 */
void LED_set_current(uint16_t current);

/*
 * @brief Returns the last current set via LED_set_current().
 * @return Current in mA
 */
uint16_t LED_get_current(void);

// ─────────────────────────────────────────────
// PWM Hardware Layer
// ─────────────────────────────────────────────

/*
 * @brief Writes a duty cycle value to a PWM channel.
 *        Handles VDD scaling and timer register write.
 * @param pwm_channel  Pointer to PWM channel config
 * @param duty_cycle   Duty cycle value (0-99 for LED channels, 0-399 for buck)
 */
void set_pwm_duty_cycle(const PWM_Config *pwm_channel, uint16_t duty_cycle);


//enable led boost converter
void enable_led_boost(void);

//disable led boost converter
void disable_led_boost(void);

void LED_flash_start(uint16_t on_ms);
void LED_flash_stop(void);


#endif