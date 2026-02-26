#include "ti_msp_dl_config.h"
#include "HAL/uart.h"
#include <stdlib.h>
#include <string.h>
#include "HAL/i2c.h"
#include "ics/BQ25628/BQ25628_functions.h"
#include "HAL/spi_slave.h"
extern volatile bool bq_monitor_active; 
void cmd_pwr(char *args) {
    char *tokens[2];
    int tokenCount = CLI_Tokenize(args, tokens, 2);

    if (tokenCount < 2) {
        uart_printf("Usage: pwr <rail_name> <1|0>\n");
        return;
    }

    char *rail = tokens[0];
    int state = atoi(tokens[1]);

    if (strcmp(rail, "3v8") == 0) {
        if(state) DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_EN3V8_PIN);
        else      DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_EN3V8_PIN);
    } 
    else if (strcmp(rail, "lora") == 0) {
        // LORA_PON handles both 3V2 and PWR rails per your table
        if(state) DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_PON_PIN);
        else      DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_PON_PIN);
    }
    else if (strcmp(rail, "lte") == 0) {
        if(state) DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_MCU_LTE_PON_PIN);
        else      DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_MCU_LTE_PON_PIN);
    }
    else if (strcmp(rail, "wifi") == 0) {
        if(state) DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_MCU_WIFI_PON_PIN);
        else      DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_MCU_WIFI_PON_PIN);
    }
    else if (strcmp(rail, "stm") == 0) {
        if(state) DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_STM_PON_PIN);
        else      DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_STM_PON_PIN);
    }
    else {
        uart_printf("Unknown rail: %s\n", rail);
        return;
    }

    uart_printf("Power %s %s\n", rail, state ? "ENABLED" : "DISABLED");
}

void cmd_i2cscan(char *args) {
    char *tokens[1];
    int tokenCount = CLI_Tokenize(args, tokens, 1);
    
    I2C_Regs *targetBus;
    int busNum = (tokenCount > 0) ? atoi(tokens[0]) : 0; // Default to Bus 0

    if (busNum == 0)      targetBus = I2C_0_INST;
    else if (busNum == 1) targetBus = I2C_1_INST;
    else {
        uart_printf("Invalid bus. Use 0 or 1.\n");
        return;
    }

    uart_printf("Scanning I2C Bus %d...\n", busNum);
    uint8_t foundCount = 0;

    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (I2C_TryAddress(targetBus, addr)) {
            uart_printf("  Found device at 0x%02X\n", addr);
            foundCount++;
        }
    }

    if (foundCount == 0) {
        uart_printf("No devices found.\n");
    } else {
        uart_printf("Scan complete. %d device(s) found.\n", foundCount);
    }
}


void cmd_hall(char *args) {
    char *tokens[2];
    int tokenCount = CLI_Tokenize(args, tokens, 2);

    if (tokenCount < 1) {
        uart_printf("Usage: hall <pwr|status> [value]\n");
        uart_printf("  hall pwr 1    - Enable 3V_HALL power\n");
        uart_printf("  hall pwr 0    - Disable 3V_HALL power\n");
        uart_printf("  hall status   - Monitor SETUP_INT at 200ms (press any key to stop)\n");
        return;
    }

    char *subcmd = tokens[0];
    
    if (strcmp(subcmd, "pwr") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: hall pwr <1|0>\n");
            return;
        }
        int state = atoi(tokens[1]);
        
        if (state) {
            DL_GPIO_setPins(DIGITAL_OUTPUT_PORTA_PORT, DIGITAL_OUTPUT_PORTA_HALL_3V_PIN);
            uart_printf("3V_HALL power ENABLED\n");
        } else {
            DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTA_PORT, DIGITAL_OUTPUT_PORTA_HALL_3V_PIN);
            uart_printf("3V_HALL power DISABLED\n");
        }
    }
    else if (strcmp(subcmd, "status") == 0) {
        extern volatile bool hall_monitor_active;
        
        if (hall_monitor_active) {
            // If already monitoring, stop it
            uart_printf("SETUP_INT monitoring stopped\n");
            hall_monitor_active = false;
        } else {
            // Start monitoring at fixed 200ms rate
            uart_printf("Monitoring SETUP_INT at 200ms rate (press any key to stop)\n");
            hall_monitor_active = true;
        }
    }
    else {
        uart_printf("Unknown subcommand: %s\n", subcmd);
    }
}


void cmd_bq(char *args) {
    char *tokens[4];
    int tokenCount = CLI_Tokenize(args, tokens, 4);

    if (tokenCount == 0) {
        uart_printf("BQ25628E Bring-up CLI\n"
                    "  bq init             - full charger init\n"
                    "  bq read <reg> [len] - read 8-bit (hex reg)\n"
                    "  bq read16 <reg>     - read 16-bit\n"
                    "  bq dump             - read all key registers\n"
                    "  bq write <reg> <val>- write 8-bit\n"
                    "  bq write16 <reg> <val> - write 16-bit\n"
                    "  bq enable           - start charging (EN_CHG + CE=LOW)\n"
                    "  bq disable          - stop charging (EN_CHG=0 + CE=HIGH)\n"
                    "  bq monitor          - 200ms status + flag monitor\n"
                    "  bq stop             - stop monitor\n" );
        return;
    }

    char *sub = tokens[0];

/* Charger initialization */
    if (strcmp(sub, "init") == 0) {
        // Pre-check device presence
        uart_printf("Checking for BQ25628E...\n");
        if (!I2C_TryAddress(I2C_0_INST, BQ25628E_I2C_ADDR)) {
            uart_printf("ERROR: Device not found at 0x%02X\n", BQ25628E_I2C_ADDR);
            uart_printf("Run 'i2cscan 0' to see available devices\n");
            return;
        }
        uart_printf("Device found! Initializing...\n");
    if (BQ25628E_Init_Default()) {
        uart_printf("Charger initialized successfully\n");
    } else {
        uart_printf("ERROR: Charger initialization failed\n");
    }
    }
    /* Read single register / register set (8-bit) */
    else if (strcmp(sub, "read") == 0 && tokenCount >= 2) {
        uint8_t reg = (uint8_t)strtol(tokens[1], NULL, 16);
        uint8_t len = (tokenCount >= 3) ? (uint8_t)atoi(tokens[2]) : 1;
        uart_printf("0x%02X = ", reg);
        for (uint8_t i = 0; i < len; i++) {
            uint8_t v = BQ25628E_ReadReg8(reg + i);
            uart_printf("0x%02X (%3d)  ", v, v);
        }
        uart_printf("\n");
    }

    /* Read 16-bit register */
    else if (strcmp(sub, "read16") == 0 && tokenCount >= 2) {
        uint8_t reg = (uint8_t)strtol(tokens[1], NULL, 16);
        uint16_t v = BQ25628E_ReadReg16(reg);
        uart_printf("0x%02X (16-bit) = 0x%04X (%5d)\n", reg, v, v);
    }

    /* Read ALL configuration registers */
    else if (strcmp(sub, "dump") == 0) {
        uart_printf("=== BQ25628E Full Register Dump ===\n");
        uart_printf("ICHG   0x02: 0x%04X\n", BQ25628E_ReadReg16(0x02));
        uart_printf("VREG   0x04: 0x%04X\n", BQ25628E_ReadReg16(0x04));
        uart_printf("IINDPM 0x06: 0x%04X\n", BQ25628E_ReadReg16(0x06));
        uart_printf("VINDPM 0x08: 0x%04X\n", BQ25628E_ReadReg16(0x08));
        uart_printf("VSYSMIN0x0E: 0x%04X\n", BQ25628E_ReadReg16(0x0E));
        uart_printf("CTRL0  0x16: 0x%02X\n", BQ25628E_ReadReg8(0x16));
        uart_printf("CTRL1  0x17: 0x%02X\n", BQ25628E_ReadReg8(0x17));
        uart_printf("CTRL3  0x19: 0x%02X\n", BQ25628E_ReadReg8(0x19));
        uart_printf("NTC0   0x1A: 0x%02X\n", BQ25628E_ReadReg8(0x1A));
        uart_printf("STAT0  0x1D: 0x%02X\n", BQ25628E_ReadReg8(0x1D));
        uart_printf("STAT1  0x1E: 0x%02X  [CHG_STAT[4:3]=0b%02b]\n",
                    BQ25628E_ReadReg8(0x1E), (BQ25628E_ReadReg8(0x1E)>>3)&0x03);
        uart_printf("CHG_FLAG0 0x20: 0x%02X\n", BQ25628E_ReadReg8(0x20));
        uart_printf("FAULT_FLAG0 0x22: 0x%02X\n", BQ25628E_ReadReg8(0x22));
    }

    /* Write register / register set */
    else if (strcmp(sub, "write") == 0 && tokenCount >= 3) {
        uint8_t reg = (uint8_t)strtol(tokens[1], NULL, 16);
        uint8_t val = (uint8_t)strtol(tokens[2], NULL, 0);
        BQ25628E_WriteReg8(reg, val);
        uart_printf("Wrote 0x%02X to 0x%02X\n", val, reg);
    }
    else if (strcmp(sub, "write16") == 0 && tokenCount >= 3) {
        uint8_t reg = (uint8_t)strtol(tokens[1], NULL, 16);
        uint16_t val = (uint16_t)strtol(tokens[2], NULL, 0);
        BQ25628E_WriteReg16(reg, val);
        uart_printf("Wrote 0x%04X to 0x%02X (16-bit)\n", val, reg);
    }

    /* Enable Charging */
    else if (strcmp(sub, "enable") == 0) {
        // BQ25628E_Set_ChargerEnable(true);
        DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_CHARGER_EN_PIN); 
        uart_printf("Charging STARTED (EN_CHG=1 + CE=LOW)\n");
    }

    /* Disable Charging */
    else if (strcmp(sub, "disable") == 0) {
        // BQ25628E_Set_ChargerEnable(false);
        DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_CHARGER_EN_PIN); 
        uart_printf("Charging STOPPED (EN_CHG=0 + CE=HIGH)\n");
    }

    /* Monitor charging status and charger flag */
    else if (strcmp(sub, "monitor") == 0) {
        bq_monitor_active = true;
        uart_printf("Monitor started (200ms) — type any command to stop\n");
    }

    /* Stop monitoring */
    else if (strcmp(sub, "stop") == 0) {
        bq_monitor_active = false;
        uart_printf("Monitor stopped\n");
    }

    else {
        uart_printf("Unknown bq sub-command. Type 'bq' for help.\n");
    }
}

void cmd_spi(char *args) {
    char *tokens[3];
    int tokenCount = CLI_Tokenize(args, tokens, 3);

    if (tokenCount == 0) {
        uart_printf("SPI Slave Testing CLI:\n"
                    "  spi init                - Reset DMA/FIFOs and Arm\n"
                    "  spi tx_view             - View current 16-byte Outbox\n"
                    "  spi tx_write <idx> <val>- Update byte in Outbox\n"
                    "  spi monitor             - Live log incoming STM32 data\n");
        return;
    }

    char *sub = tokens[0];

    /* 1. INIT (ARM) */
    if (strcmp(sub, "init") == 0) {
        SPI_Peripheral_Arm(&stm32Spi);
        uart_printf("SPI Slave Initialized and Armed (Ready for STM32)\n");
    }

    /* 2. TX_VIEW */
    else if (strcmp(sub, "tx_view") == 0) {
        uart_printf("Current TX Buffer (Outbox):\n");
        for (int i = 0; i < stm32Spi.size; i++) {
            uart_printf("0x%02X ", stm32Spi.txBuf[i]);
            if ((i + 1) % 8 == 0) uart_printf("\n");
        }
    }

    /* 3. TX_WRITE */
    else if (strcmp(sub, "tx_write") == 0 && tokenCount >= 3) {
        uint8_t idx = (uint8_t)atoi(tokens[1]);
        uint8_t val = (uint8_t)strtol(tokens[2], NULL, 0);

        if (idx < stm32Spi.size) {
            stm32Spi.txBuf[idx] = val;
            uart_printf("Updated TX Buffer[%d] to 0x%02X\n", idx, val);
        } else {
            uart_printf("Error: Index out of bounds (0-15)\n");
        }
    }

    /* 4. MONITOR */
    else if (strcmp(sub, "monitor") == 0) {
        uart_printf("Monitoring SPI (STM32 -> MSP)... Press any key to stop.\n");
        
        // Ensure we are armed before starting
        SPI_Peripheral_Arm(&stm32Spi);

        while (1) {
            if (stm32Spi.transferComplete) {
                uart_printf("Received from STM32:\n");
                for (int i = 0; i < stm32Spi.size; i++) {
                    uart_printf("%02X ", stm32Spi.rxBuf[i]);
                }
                uart_printf("\n---\n");

                // Auto-rearm for the next packet
                SPI_Peripheral_Arm(&stm32Spi);
            }

            // Check for UART input to break the loop (same logic as your BQ monitor)
            if (DL_UART_Main_isRXFIFOEmpty(UART_0_INST) == false) {
                DL_UART_Main_receiveData(UART_0_INST);
                uart_printf("SPI Monitor Stopped.\n");
                break;
            }
        }
    }
    else {
        uart_printf("Unknown SPI sub-command.\n");
    }
}