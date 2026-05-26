/***************************************************************************//**
 * @file
 * @brief PYD1588 PIR sensor driver for MSPM0
 ******************************************************************************/
#include "pyd1588.h"
#include "ti_msp_dl_config.h"
#include "HAL/uart.h"

// Microsecond delay for 32MHz CPU clock (32 cycles per us)
// SysConfig shows BUSCLK at 32MHz. If 4MHz, change 32 to 4.
#define delay_us(x) delay_cycles((uint32_t)(x * 32))

static uint32_t s_last_config = PIR_INIT_VALUE;

void PIR_writeConfig(uint32_t regval) {
    s_last_config = regval;
    int i;
    uint32_t regmask = 0x1000000;

    // Hold DIRECT LINK LOW for entire duration - do NOT release until after tSLT
    DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);

    DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
    delay_us(2);

    __disable_irq();

    for (i = 0; i < 25; i++) {
        bool nextbit = (regval & regmask) != 0;
        regmask >>= 1;

        DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        delay_us(1);
        DL_GPIO_setPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        delay_us(1);

        if (nextbit) {
            DL_GPIO_setPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        } else {
            DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        }
        delay_us(100); // 100us hold time matching STM32 exactly
    }

    // Pull SERIN low to trigger latch
    DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
    
    // tSLT must complete BEFORE releasing DIRECT LINK
    delay_us(700);

    __enable_irq();

    // Only NOW release DIRECT LINK - config is safely latched
    DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
}

int PIR_readData(uint32_t *statcfg_out, bool *out_of_range) {
    int i;
    int pirVal = 0;
    uint32_t statcfg = 0;
    uint32_t uibitmask = 0x4000;
    uint32_t ulbitmask = 0x1000000;
    bool oor = false;
    __disable_irq();
    
    // Check if the sensor is actively driving an interrupt (Wake Up mode)
    bool is_wakeup = DL_GPIO_readPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN) != 0;

    // Pull LOW for 200us first to reset the sensor's internal readout state machine/MDU
    DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    delay_us(200);

    // Drive HIGH for tDS >= 120us (setup pulse)
    DL_GPIO_setPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    delay_us(130);

    for (i = 0; i < 15; i++) {
        DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(1); // Safer clock LOW
        DL_GPIO_setPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(1); // Safer clock HIGH
        DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(15); // Safer settling time for high line capacitance

        if (DL_GPIO_readPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN)) {
            pirVal |= uibitmask;
        }
        uibitmask >>= 1;
    }

    for (i = 0; i < 25; i++) {
        DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(1); // Safer clock LOW
        DL_GPIO_setPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(1); // Safer clock HIGH
        DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(15); // Safer settling time for high line capacitance

        if (DL_GPIO_readPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN)) {
            statcfg |= ulbitmask;
        }
        ulbitmask >>= 1;
    }

    __enable_irq();

    DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    delay_us(1300);
    DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);

    oor = (pirVal & 0x4000) == 0;
    
    if (is_wakeup) {
        // In Wake Up mode, the sensor outputs Bit 39 during the setup pulse.
        // Our clock-then-read loop skips Bit 39, shifting the data left by 1.
        // We reconstruct the data by shifting right by 1.
        uint8_t bit24 = pirVal & 1;
        pirVal >>= 1;
        statcfg >>= 1;
        if (bit24) {
            statcfg |= 0x1000000;
        }
        oor = false; // Wake Up interrupt only fires for valid motion (Normal Operation)
    }

    pirVal &= 0x3FFF;

    if (!(statcfg & 0x60)) {
        if (pirVal & 0x2000) {
            pirVal -= 0x4000;
        }
    }

    if (out_of_range) *out_of_range = oor;
    if (statcfg_out) *statcfg_out = statcfg;
    return pirVal;
}

void PIR_clearInterrupt(void) {
    // To clear interrupt on PYD, pull the link low for ~40us
    // Datasheet: To reset the Motion Detection Unit, pull LOW for at least 160us.
    DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    delay_us(200);
    DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
}

int PIR_configureAndVerify(uint32_t config) {
    uint32_t cfg_read;
    // Force Operation Mode 0 (Forced Readout) for verification configuration
    uint32_t cfg_verify = (config & ~((uint32_t)0x03 << 7)) | (0 << 7);

    // Step 1: Write verification config
    PIR_writeConfig(cfg_verify);
    delay_us(3000); // Wait for settle (min 2.4ms)

    // Step 2: Read back and verify register in Forced Readout mode
    PIR_readData(&cfg_read, NULL);
    PIR_clearInterrupt();

    // Check if configuration read matches verification config
    if ((cfg_read & 0x1FFFFFF) != (cfg_verify & 0x1FFFFFF)) {
        uart_printf("[PYD2] Config verification failed!\n");
        uart_printf("  Configured (Forced Readout): 0x%08X\n", (unsigned int)(cfg_verify & 0x1FFFFFF));
        uart_printf("  Read back from sensor      : 0x%08X\n", (unsigned int)(cfg_read & 0x1FFFFFF));
        return -1; // Verification failed
    }

    // Step 3: Write final Wake Up configuration
    PIR_writeConfig(config);
    delay_us(3000); // Wait for settle (min 2.4ms)
    PIR_clearInterrupt();

    return 0; // Success
}

int PIR_init(void) {
    return PIR_configureAndVerify(PIR_INIT_VALUE);
}

void PIR_enableForcedRead(bool enable) {
    if (enable) {
        uint32_t forced_cfg = (s_last_config & ~((uint32_t)0x03 << 7)) | (0 << 7);
        uint32_t saved = s_last_config;
        PIR_writeConfig(forced_cfg);
        s_last_config = saved; // Keep the intended config
    } else {
        PIR_writeConfig(s_last_config);
        delay_us(3000); // Wait for settle (min 2.4ms)
        PIR_clearInterrupt();
    }
}