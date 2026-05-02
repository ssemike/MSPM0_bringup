#include "BoardAPI.h"
#include "ti_msp_dl_config.h"
#include "ti/driverlib/dl_timerg.h"
#include <stdint.h>
#include <stdlib.h>

// ─────────────────────────────────────────────
// PWM Channel Definitions
// ─────────────────────────────────────────────

// Index 0: LED boost converter (voltage control)
// Index 1: LED1 current control
static const PWM_Config _pwm_outputs[3] = {
    {BOOST_CONTROL_INST, GPIO_BOOST_CONTROL_C0_IDX, 0},  // boost converter → voltage
    {BOOST_CONTROL_INST, GPIO_BOOST_CONTROL_C2_IDX, 0},  // LED1             → current
    {FLASH_CONTROL_INST, GPIO_FLASH_CONTROL_C1_IDX, 0},  // Flash LED         → current

};
// ─────────────────────────────────────────────
// Current Lookup Tables
// ─────────────────────────────────────────────

// Lookup Tables for Boost output voltage
uint8_t duty_cycles_boost[MAX_DUTY_CYCLES_BOOST] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35
};

uint16_t output_voltages_boost_mV[MAX_DUTY_CYCLES_BOOST] = {
    11330, 11070, 10880, 10570, 10330,
    10070, 9820, 9570, 9320, 9100,
    9070, 8830, 8580, 8310, 8060,
    7810, 7300, 7070, 6800, 6550,
    6540, 6290, 6030, 5780, 5580,
    5268, 5015, 4757, 4700, 4050,
    4244, 3995, 3994, 3734, 3490
};

// Lookup Tables for LED Output current
uint8_t duty_cycles_led[MAX_DUTY_CYCLES_LED] = {
    0, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
    51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
    71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
    81, 82, 83, 84, 85, 86, 87, 88, 89, 90,
    91, 92, 93, 94, 95, 96, 97, 98, 99
};


uint16_t output_currents_led_mA[MAX_DUTY_CYCLES_LED] = {
    0, 16, 44, 74, 104, 132, 162, 192, 220, 250,
    280, 310, 338, 368, 396, 426, 454, 486, 514, 542,
    572, 602, 632, 662, 692, 722, 752, 782, 812, 842,
    872, 902, 932, 962, 992, 1022, 1052, 1082, 1112, 1142,
    1172, 1202, 1232, 1262, 1292, 1322, 1352, 1382, 1412, 1442,
    1472, 1502, 1532, 1562, 1592, 1622, 1652, 1682, 1712, 1742,
    1772, 1802, 1832, 1862, 1892, 1922, 1952, 1982, 2012, 2042,
    2072, 2102, 2132, 2162, 2192, 2222, 2252, 2282, 2312, 2342,
    2372, 2402, 2432, 2462, 2492, 2522, 2552, 2582, 2612, 2642,
    2672, 2702, 2732, 2762, 2792, 2822, 2852, 2882, 2912
};


// ─────────────────────────────────────────────
// LED State
// ─────────────────────────────────────────────

led_channel_t led_channel = { .set_current = 0 };
uint16_t global_led_voltage = 0;

// ─────────────────────────────────────────────
// Internal Helpers
// ─────────────────────────────────────────────

// Binary search on an ascending sorted LUT.
// Returns the index of the entry closest to the requested value.
static uint16_t _binary_search_ascending(uint16_t value, const uint16_t *LUT, uint16_t table_size) {
    int16_t left = 0, right = table_size - 1;
    int16_t closest_index = 0;

    while (left <= right) {
        int16_t mid = left + (right - left) / 2;

        uint16_t diff_mid     = (uint16_t)abs((int16_t)LUT[mid]           - (int16_t)value);
        uint16_t diff_closest = (uint16_t)abs((int16_t)LUT[closest_index] - (int16_t)value);

        if (diff_mid < diff_closest) closest_index = mid;

        if      (LUT[mid] == value) return mid;
        else if (LUT[mid] <  value) left  = mid + 1;
        else                        right = mid - 1;
    }

    return closest_index;
}

// Binary search on a descending sorted LUT.
// Returns the index of the entry closest to the requested value.
// Used for the boost voltage table which goes high to low.
static uint16_t _binary_search_descending(uint16_t value, const uint16_t *LUT, uint16_t table_size) {
    int16_t left = 0, right = table_size - 1;
    int16_t closest_index = 0;

    while (left <= right) {
        int16_t mid = left + (right - left) / 2;

        uint16_t diff_mid     = (uint16_t)abs((int16_t)LUT[mid]           - (int16_t)value);
        uint16_t diff_closest = (uint16_t)abs((int16_t)LUT[closest_index] - (int16_t)value);

        if (diff_mid < diff_closest) closest_index = mid;

        if      (LUT[mid] == value) return mid;
        else if (LUT[mid] >  value) left  = mid + 1;  // descending: go right if too high
        else                        right = mid - 1;
    }

    return closest_index;
}

// ─────────────────────────────────────────────
// PWM Hardware Layer
// ─────────────────────────────────────────────

void set_pwm_duty_cycle(const PWM_Config *pwm_channel, uint16_t duty_cycle) {
    if (pwm_channel == &_pwm_outputs[2]) {
        DL_TimerA_stopCounter(pwm_channel->TIMER);
        DL_TimerA_setCaptureCompareValue(
            pwm_channel->TIMER,
            duty_cycle,
            pwm_channel->CC_INDEX
        );
        DL_TimerA_startCounter(pwm_channel->TIMER);
    } else {
        DL_TimerA_stopCounter(pwm_channel->TIMER);
        DL_TimerA_setCaptureCompareValue(
            pwm_channel->TIMER,
            100 - duty_cycle,
            pwm_channel->CC_INDEX
        );
        DL_TimerA_startCounter(pwm_channel->TIMER);
    }
}

// ─────────────────────────────────────────────
// Voltage Control
// ─────────────────────────────────────────────

void LED_set_voltage(uint16_t voltage) {
    const uint16_t LED_VMAX = 11540;
    const uint16_t v_d = 92;
    
    uint16_t duty_cycle = (LED_VMAX - voltage) / v_d;

    if(duty_cycle > 99) duty_cycle = 99;
    if(duty_cycle < 1)  duty_cycle = 1;

    global_led_voltage = voltage;
    set_pwm_duty_cycle(&_pwm_outputs[0], duty_cycle);
}

uint16_t LED_get_voltage(void) {
    return global_led_voltage;
}

// ─────────────────────────────────────────────
// Current Control
// ─────────────────────────────────────────────

void LED_set_current(uint16_t current) {
    // Clamp to hardware maximum
    if (current > LED_HW_MAX_CURRENT_MA) current = LED_HW_MAX_CURRENT_MA;

    // Find the closest matching current in the LUT
    uint16_t index = _binary_search_ascending(current, output_currents_led_mA, MAX_DUTY_CYCLES_LED);

    // Record the actual set current (what the LUT entry gives, not the raw request)
    led_channel.set_current = output_currents_led_mA[index];

    // Apply duty cycle for LED1 channel
    set_pwm_duty_cycle(&_pwm_outputs[1], duty_cycles_led[index]);
}

uint16_t LED_get_current(void) {
    return (uint16_t)led_channel.set_current;
}

// ─────────────────────────────────────────────
// Initialisation
// ─────────────────────────────────────────────

void LED_control_init(void) {
    led_channel.set_current = 0;
    global_led_voltage      = 0;

    // Start with both PWM outputs at zero / off
    LED_set_current(0);
    LED_set_voltage(11330);
}


void enable_led_boost(void){
    DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_IR_ENABLE_PIN);
};


void disable_led_boost(void){
    DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_IR_ENABLE_PIN);
};

void LED_flash_start(uint16_t on_ms) {
    if (on_ms < 1)  on_ms = 1;
    if (on_ms > 50) on_ms = 50;

    uint16_t ticks = on_ms * 500;
    set_pwm_duty_cycle(&_pwm_outputs[2], ticks);
}

void LED_flash_stop(void) {
    DL_TimerA_stopCounter(FLASH_CONTROL_INST);
}