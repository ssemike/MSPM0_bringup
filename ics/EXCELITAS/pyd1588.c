/***************************************************************************//**
 * @file
 * @brief PYD1588 PIR sensor driver for MSPM0
 ******************************************************************************/
#include "pyd1588.h"
#include "ti_msp_dl_config.h"

// Microsecond delay for 32MHz CPU clock (32 cycles per us)
// SysConfig shows BUSCLK at 32MHz. If 4MHz, change 32 to 4.
#define delay_us(x) delay_cycles((uint32_t)(x * 32))

void PIR_writeConfig(uint32_t regval) {
    int i;
    uint32_t regmask = 0x1000000; // 25th bit

    // Ensure we start low
    DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
    
    for (i = 0; i < 25; i++) {
        bool nextbit = (regval & regmask) != 0;
        regmask >>= 1;

        // Start bit pulse (Request bit)
        DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        DL_GPIO_setPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);

        // Data setup
        if (nextbit) {
            DL_GPIO_setPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        } else {
            DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
        }

        delay_us(100);
    }
    DL_GPIO_clearPins(GPIOB, DIGITAL_OUTPUT_PORTB_SERIN_PIN);
    delay_us(650);
}

int PIR_readData(uint32_t *statcfg_out) {
    int i;
    uint32_t uibitmask = 0x4000; // 15th bit
    uint32_t ulbitmask = 0x1000000; // 25th bit
    int pirVal = 0;
    uint32_t statcfg = 0;

    // Wake up/Start reading session
    // Switch to output to drive the "Start" pulse
    DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    DL_GPIO_setPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);

    delay_us(130);

    // Get 15 bits of ADC data
    for (i = 0; i < 15; i++) {
        // Trigger next bit: Pulse Low then High
        DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(2); // Short pulse
        
        DL_GPIO_setPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        
        // Switch to input to read back
        DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(5); // Wait for sensor to drive the line

        if (DL_GPIO_readPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN)) {
            pirVal |= uibitmask;
        }
        uibitmask >>= 1;
    }

    // Get 25 bits of status/config
    for (i = 0; i < 25; i++) {
        DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(2);
        
        DL_GPIO_setPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        
        DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        delay_us(5);

        if (DL_GPIO_readPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN)) {
            statcfg |= ulbitmask;
        }
        ulbitmask >>= 1;
    }

    // End reading session: ensure pin is back to input/interrupt mode
    DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    
    // Minimum time to allow registers to update before next read
    delay_us(1300);

    pirVal &= 0x3FFF; // Clear unused bits

    // Convert to signed if using Bandpass
    if (!(statcfg & 0x60)) {
        if (pirVal & 0x2000) {
            pirVal -= 0x4000;
        }
    }

    if (statcfg_out) {
        *statcfg_out = statcfg;
    }
    return pirVal;
}

void PIR_clearInterrupt(void) {
    // To clear interrupt on PYD, pull the link low for ~40us
    DL_GPIO_enableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    DL_GPIO_clearPins(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    delay_us(45);
    DL_GPIO_disableOutput(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
}

int PIR_init(void) {
    uint32_t cfg_read;
    uint32_t cfg = PIR_INIT_VALUE;

    PIR_writeConfig(cfg);
    delay_us(2000);
    PIR_readData(&cfg_read);
    PIR_clearInterrupt();

    // Mask for relevant config bits (last 25 bits)
    return ((cfg_read & 0x1FFFFFF) == (cfg & 0x1FFFFFF)) ? 0 : -1;
}