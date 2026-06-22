#include "ti_msp_dl_config.h"
#include "HAL/uart.h"
#include <stdlib.h>
#include <string.h>
#include "HAL/i2c.h"
#include "ics/BQ25628/BQ25628_functions.h"
#include "HAL/spi_master.h"
#include "ics/BQ27Z7/BQ27Z7_functions.h"
#include "HAL/spi_mem.h"
#include "ics/MB85RS/mb85rs.h"
#include "BoardAPI.h"
#include "HAL/i2c.h"
#include "ics/ZILOG/ZDP323B.h"
#include "ics/LTR329/LTR329.h"
#include "ics/LIS3DH/LIS3DH.h"
#include "HAL/SX126x_MSPM0.h"
#include "ics/IMX335/IMX335.h"
#include "ics/SX126X/sx126x-board.h"
#include "ics/SX126X/radio.h"



extern volatile bool gauge_monitor_active;
extern volatile bool bq_monitor_active; 
extern volatile bool pir_monitor_active;
extern volatile bool lis_monitor_active;

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
        DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_CHARGER_EN_PIN); 
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
        uart_printf("Monitor started  — type any command to stop\n");
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
        uart_printf("SPI Master Testing CLI:\n"
                    "  spi tx_view             - View current 16-byte Outbox\n"
                    "  spi tx_write <idx> <val>- Update byte in Outbox\n"
                    "  spi test            - test comms over SPI\n" );
        return;
    }

    char *sub = tokens[0];

    /* 2. TX_VIEW */
    if (strcmp(sub, "tx_view") == 0) {
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
    else if (strcmp(sub, "test") == 0) {
        uart_printf("Communicating over SPI (MSP -> STM32)... Press any key to stop.\n");
        SPI_Controller_Arm(&stm32Spi);
        while (1) {
            if (stm32Spi.rxDone) {
                uart_printf("Received from STM32:\n");
                for (int i = 0; i < stm32Spi.size; i++) {
                    uart_printf("%02X ", stm32Spi.rxBuf[i]);
                }
                uart_printf("\n---\n");
                stm32Spi.rxDone = false;
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

static void print_battery_status(uint16_t status)
{
    uart_printf("BatteryStatus: 0x%04X\n", status);
    uart_printf("  FC  (Fully Charged)    : %s\n", (status & BQ27Z746_STATUS_FC)   ? "YES" : "no");
    uart_printf("  FD  (Fully Discharged) : %s\n", (status & BQ27Z746_STATUS_FD)   ? "YES" : "no");
    uart_printf("  DSG (Discharging)      : %s\n", (status & BQ27Z746_STATUS_DSG)  ? "YES" : "no");
    uart_printf("  INIT (Initializing)    : %s\n", (status & BQ27Z746_STATUS_INIT) ? "YES" : "no");
    uart_printf("  RCA (RemainingCap Alm) : %s\n", (status & BQ27Z746_STATUS_RCA)  ? "YES" : "no");
    uart_printf("  TDA (TermDischarge Alm): %s\n", (status & BQ27Z746_STATUS_TDA)  ? "YES" : "no");
}

static void print_time(uint16_t minutes)
{
    if (minutes == 0xFFFFu)
        uart_printf("  ---");
    else
        uart_printf("%4dmin", minutes);
}


MB85RS_Handle fram;

void cmd_fram(char *args)
{
    char *tokens[11];   /* sub + addr + up to 8 data bytes + spare */
    int   tokenCount = CLI_Tokenize(args, tokens, 11);

    if (tokenCount == 0) {
        uart_printf("MB85RS2MTA FRAM CLI\n"
                    "  fram init               - init driver and verify device ID\n"
                    "  fram id                 - read raw 4-byte device ID\n"
                    "  fram status             - decode status register\n"
                    "  fram read  <addr> <len> - hex dump (hex values, 0x prefix optional)\n"
                    "  fram write <addr> <b0> [b1..b7] - write bytes\n");
        return;
    }

    char *sub = tokens[0];

    /* ------------------------------------------------------------------ */
    /* fram init                                                            */
    /* ------------------------------------------------------------------ */
    if (strcmp(sub, "init") == 0) {
        uart_printf("Initialising FRAM...\n");

        MB85RS_Error err = MB85RS_Init(&fram, &framSpi);

        if (err == MB85RS_OK) {
            uart_printf("FRAM OK - MB85RS2MTA found and confirmed\n");
        } else if (err == MB85RS_ERR_DEVICE_ID) {
            uart_printf("ERROR: Device ID mismatch - wrong device or wiring issue\n");
        } else {
            uart_printf("ERROR: Init failed (code %d)\n", (int)err);
        }
    }

    /* ------------------------------------------------------------------ */
    /* fram id                                                              */
    /* ------------------------------------------------------------------ */
    else if (strcmp(sub, "id") == 0) {
        uint8_t tx[MB85RS_RDID_LEN];
        uint8_t rx[MB85RS_RDID_LEN];

        memset(tx, 0x00, sizeof(tx));
        tx[0] = MB85RS_CMD_RDID;

        SPI_Memory_CS_Assert(fram.spi);
        SPI_Memory_Arm(fram.spi, tx, rx, MB85RS_RDID_LEN, SPI_MEM_MODE_FULL_DUPLEX);

        if (!SPI_Memory_Wait(fram.spi)) {
            SPI_Memory_CS_Deassert(fram.spi);
            uart_printf("ERROR: SPI transfer failed\n");
            return;
        }

        SPI_Memory_CS_Deassert(fram.spi);

        /* rx[0] is garbage (clocked during op-code byte) */
        uart_printf("Device ID raw bytes:\n");
        uart_printf("  [0] Manufacturer : 0x%02X  %s\n",
                    rx[1], (rx[1] == MB85RS_ID_MANUFACTURER) ? "(OK)" : "(MISMATCH)");
        uart_printf("  [1] Continuation : 0x%02X  %s\n",
                    rx[2], (rx[2] == MB85RS_ID_CONTINUATION) ? "(OK)" : "(MISMATCH)");
        uart_printf("  [2] Product ID 1 : 0x%02X  %s\n",
                    rx[3], (rx[3] == MB85RS_ID_PRODUCT1)     ? "(OK)" : "(MISMATCH)");
        uart_printf("  [3] Product ID 2 : 0x%02X  %s\n",
                    rx[4], (rx[4] == MB85RS_ID_PRODUCT2)     ? "(OK)" : "(MISMATCH)");
    }

    /* ------------------------------------------------------------------ */
    /* fram status                                                          */
    /* ------------------------------------------------------------------ */
    else if (strcmp(sub, "status") == 0) {
        uint8_t sr = 0;
        MB85RS_Error err = MB85RS_ReadStatus(&fram, &sr);

        if (err != MB85RS_OK) {
            uart_printf("ERROR: ReadStatus failed (code %d)\n", (int)err);
            return;
        }

        const char *bp_desc;
        switch (sr & (MB85RS_SR_BP1 | MB85RS_SR_BP0)) {
            case 0x00: bp_desc = "None (all writable)";          break;
            case 0x04: bp_desc = "0x30000-0x3FFFF (upper 1/4)"; break;
            case 0x08: bp_desc = "0x20000-0x3FFFF (upper 1/2)"; break;
            case 0x0C: bp_desc = "0x00000-0x3FFFF (all)";       break;
            default:   bp_desc = "Unknown";                      break;
        }

        uart_printf("Status Register: 0x%02X\n", sr);
        uart_printf("  WPEN (bit7) : %d - SR write-protect %s\n",
                    (sr & MB85RS_SR_WPEN) ? 1 : 0,
                    (sr & MB85RS_SR_WPEN) ? "ENABLED" : "disabled");
        uart_printf("  BP1  (bit3) : %d\n", (sr & MB85RS_SR_BP1) ? 1 : 0);
        uart_printf("  BP0  (bit2) : %d\n", (sr & MB85RS_SR_BP0) ? 1 : 0);
        uart_printf("  Protected   : %s\n", bp_desc);
        uart_printf("  WEL  (bit1) : %d - write %s\n",
                    (sr & MB85RS_SR_WEL) ? 1 : 0,
                    (sr & MB85RS_SR_WEL) ? "ENABLED" : "disabled");
    }

    /* ------------------------------------------------------------------ */
    /* fram read <addr> <len>                                               */
    /* ------------------------------------------------------------------ */
    else if (strcmp(sub, "read") == 0) {
        if (tokenCount < 3) {
            uart_printf("Usage: fram read <addr> <len>  (hex values)\n");
            return;
        }

        uint32_t address = (uint32_t)strtol(tokens[1], NULL, 0);
        uint32_t len     = (uint32_t)strtol(tokens[2], NULL, 0);

        /* Driver limit: 4-byte header + data must fit in 512-byte buffer */
        if (len == 0 || len > 508U) {
            uart_printf("ERROR: len must be 1-508\n");
            return;
        }

        uint8_t buf[508];
        MB85RS_Error err = MB85RS_Read(&fram, address, buf, len);

        if (err != MB85RS_OK) {
            uart_printf("ERROR: Read failed (code %d)\n", (int)err);
            return;
        }

        uart_printf("FRAM @ 0x%05X  (%u bytes):\n",
                    (unsigned int)address, (unsigned int)len);
        for (uint32_t i = 0; i < len; i++) {
            if (i % 16 == 0) {
                uart_printf("  %05X : ", (unsigned int)(address + i));
            }
            uart_printf("%02X ", buf[i]);
            if ((i % 16 == 15) || (i == len - 1)) {
                uart_printf("\n");
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* fram write <addr> <b0> [b1..b7]                                      */
    /* ------------------------------------------------------------------ */
    else if (strcmp(sub, "write") == 0) {
        if (tokenCount < 3) {
            uart_printf("Usage: fram write <addr> <b0> [b1..b7]  (hex values)\n");
            return;
        }

        uint32_t address   = (uint32_t)strtol(tokens[1], NULL, 0);
        int      dataCount = tokenCount - 2;
        if (dataCount > 8) dataCount = 8;

        uint8_t buf[8];
        for (int i = 0; i < dataCount; i++) {
            buf[i] = (uint8_t)strtol(tokens[2 + i], NULL, 0);
        }

        MB85RS_Error err = MB85RS_Write(&fram, address, buf, (uint32_t)dataCount);

        if (err != MB85RS_OK) {
            uart_printf("ERROR: Write failed (code %d)\n", (int)err);
            return;
        }

        uart_printf("Wrote %d byte(s) to 0x%05X:", dataCount, (unsigned int)address);
        for (int i = 0; i < dataCount; i++) {
            uart_printf(" 0x%02X", buf[i]);
        }
        uart_printf("\n");
    }

    else {
        uart_printf("Unknown fram sub-command. Type 'fram' for help.\n");
    }
}


/* ================================================================
 * cmd_gauge
 * ================================================================ */
void cmd_gauge(char *args)
{
    char *tokens[4];
    int tokenCount = CLI_Tokenize(args, tokens, 4);

    if (tokenCount == 0) {
        uart_printf("BQ27Z746 Gauge CLI\n"
            "  gauge on <1|0>       - Pulls ENAB_N low/high\n"
            "  gauge init           - verify comms, confirm device type\n"
            "  gauge dump           - read all telemetry registers\n"
            "  gauge status         - decode BatteryStatus bits\n"
            "  gauge info           - device type, FW version, ChemID\n"
            "  gauge read <reg>     - raw 16-bit register read\n"
            "  gauge mac <cmd>      - issue MAC command\n"
            "  gauge fet            - read FET Options DF (0x45C0)\n"
            "  gauge utfet <0|1>    - disable/enable UTFET bit\n"
            "  gauge monitor        - 200ms live telemetry\n"
            "  gauge stop           - stop monitor\n"
            "  gauge reset          - reset gauge\n"
            "  gauge security       - print current security mode\n"
            "  gauge unseal         - unseal using default keys\n"
            "  gauge seal           - seal device\n"
            "\n");
        return;
    }

    char *sub = tokens[0];

    if (strcmp(sub, "on") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: gauge on <1|0>\n");
            return;
        }
        int state = atoi(tokens[1]);
        if (state) {
            DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_GAUGE_EN_PIN);
            uart_printf("Gauge ENAB_N Enabled\n");
        } else {
            DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_GAUGE_EN_PIN);
            uart_printf("Gauge ENAB_N disabled\n");
        }
    }

    else if (strcmp(sub, "init") == 0) {
        uart_printf("Checking for BQ27Z746 on I2C0...\n");

        if (!I2C_TryAddress(I2C_0_INST, GAUGE_I2C_ADDR)) {
            uart_printf("ERROR: No device found at 0x%02X\n", GAUGE_I2C_ADDR);
            uart_printf("Run 'i2cscan 0' to check what is on the bus\n");
            return;
        }

        if (!BQ27Z746_Init(I2C_0_INST)) {
            uart_printf("ERROR: Init failed — failed init for (TS)\n");
            return;
        }

        uart_printf("BQ27Z746 found and confirmed\n");

        uint16_t fw = 0u;
        if (BQ27Z746_GetFirmwareVersion(I2C_0_INST, &fw))
            uart_printf("Firmware Version : 0x%04X\n", fw);
        else
            uart_printf("Firmware Version : read failed\n");
    }

    else if (strcmp(sub, "dump") == 0) {
        uart_printf("=== BQ27Z746 Register Dump ===\n");
        uart_printf("Voltage          [0x08]: %4d mV\n",  BQ27Z746_ReadVoltage_mV(I2C_0_INST));
        uart_printf("Current          [0x0C]: %5d mA\n",  BQ27Z746_ReadCurrent_mA(I2C_0_INST));
        uart_printf("Avg Current      [0x14]: %5d mA\n",  BQ27Z746_ReadAvgCurrent_mA(I2C_0_INST));
        uart_printf("Avg Power        [0x22]: %5d mW\n",  BQ27Z746_ReadAvgPower_mW(I2C_0_INST));
        uart_printf("SOC              [0x2C]: %3d %%\n",   BQ27Z746_ReadSOC_pct(I2C_0_INST));
        uart_printf("Remaining Cap    [0x10]: %4d mAh\n", BQ27Z746_ReadRemainingCap_mAh(I2C_0_INST));
        uart_printf("Full Charge Cap  [0x12]: %4d mAh\n", BQ27Z746_ReadFullChargeCap_mAh(I2C_0_INST));
        uart_printf("State of Health  [0x2E]: %3d %%\n",   BQ27Z746_ReadStateOfHealth_pct(I2C_0_INST));
        uart_printf("Temperature      [0x06]: %3d C\n",   BQ27Z746_ReadTemperature_C(I2C_0_INST));
        uart_printf("Internal Temp    [0x28]: %3d C\n",   BQ27Z746_ReadInternalTemp_C(I2C_0_INST));

        uint16_t tte = BQ27Z746_ReadTimeToEmpty_min(I2C_0_INST);
        uint16_t ttf = BQ27Z746_ReadTimeToFull_min(I2C_0_INST);
        uart_printf("Time to Empty    [0x16]: "); print_time(tte); uart_printf("\n");
        uart_printf("Time to Full     [0x18]: "); print_time(ttf); uart_printf("\n");

        uart_printf("Cycle Count      [0x2A]: %d\n",      BQ27Z746_ReadCycleCount(I2C_0_INST));
        uart_printf("Battery Status   [0x0A]: 0x%04X\n",  BQ27Z746_ReadBatteryStatus(I2C_0_INST));
    }

    else if (strcmp(sub, "status") == 0) {
        uint16_t status = BQ27Z746_ReadBatteryStatus(I2C_0_INST);
        print_battery_status(status);
    }

    else if (strcmp(sub, "info") == 0) {
        uart_printf("=== BQ27Z746 Device Info ===\n");

        uint16_t dev_type = 0u;
        if (BQ27Z746_GetDeviceType(I2C_0_INST, &dev_type))
            uart_printf("Device Type      : 0x%04X %s\n", dev_type,
                        (dev_type == BQ27Z746_DEVICE_TYPE) ? "(OK)" : "(MISMATCH)");
        else
            uart_printf("Device Type      : read failed\n");

        uint16_t fw = 0u;
        if (BQ27Z746_GetFirmwareVersion(I2C_0_INST, &fw))
            uart_printf("Firmware Version : 0x%04X\n", fw);
        else
            uart_printf("Firmware Version : read failed\n");

        uint16_t chem = 0u;
        if (BQ27Z746_GetChemID(I2C_0_INST, &chem))
            uart_printf("Chem ID          : 0x%04X\n", chem);
        else
            uart_printf("Chem ID          : read failed\n");

        /* Single read — split into statusA / statusB locally */
        uint32_t op_status = 0u;
        if (BQ27Z746_GetOperationStatus(I2C_0_INST, &op_status)) {
            uint16_t statusA = (uint16_t)(op_status & 0xFFFFu);
            uint16_t statusB = (uint16_t)(op_status >> 16u);
            uart_printf("Operation Status : 0x%08X\n", (unsigned int)op_status);
            uart_printf("  Status A       : 0x%04X\n", statusA);
            uart_printf("  Status B       : 0x%04X\n", statusB);
        } else {
            uart_printf("Operation Status : read failed\n");
        }

        uint8_t tempRange = 0u;
        uint16_t chgStatus = 0u;
        if (BQ27Z746_GetChargingStatus(I2C_0_INST, &tempRange, &chgStatus)) {
            uart_printf("Temp Range       : 0x%02X\n", tempRange);
            uart_printf("Chg Status       : 0x%04X\n", chgStatus);
        } else {
            uart_printf("Charging Status  : read failed\n");
        }

        uint32_t safety_status = 0u;
        if (BQ27Z746_GetSafetyStatus(I2C_0_INST, &safety_status))
            uart_printf("Safety Status    : 0x%08X\n", (unsigned int)safety_status);
        else
            uart_printf("Safety Status    : read failed\n");

        uint8_t tempCfg = 0u;
        if (BQ27Z746_GetTempConfig(I2C_0_INST, &tempCfg)) {
            uart_printf("Temp Config      : 0x%02X\n", tempCfg);
            uart_printf("  TSInt (int)    : %s\n", (tempCfg & (1u << 0)) ? "ENABLED" : "disabled");
            uart_printf("  TS1   (ext)    : %s\n", (tempCfg & (1u << 1)) ? "ENABLED" : "disabled");
            uart_printf("  TS2   (GPO)    : %s\n", (tempCfg & (1u << 2)) ? "ENABLED" : "disabled");
        } else {
            uart_printf("Temp Config      : read failed\n");
        }
    }

    else if (strcmp(sub, "read") == 0 && tokenCount >= 2) {
        uint8_t reg = (uint8_t)strtol(tokens[1], NULL, 16);
        uint16_t val = (uint16_t)gauge_cmd_read(I2C_0_INST, reg);
        uart_printf("0x%02X = 0x%04X (%d)\n", reg, val, val);
    }

    else if (strcmp(sub, "mac") == 0 && tokenCount >= 2) {
        uint16_t cmd = (uint16_t)strtol(tokens[1], NULL, 16);
        uint8_t  data[BQ27Z746_MAC_DATA_LEN];
        uint8_t  len = 0u;

        uart_printf("Sending MAC cmd 0x%04X...\n", cmd);

        if (!BQ27Z746_MAC_Read(I2C_0_INST, cmd, data, &len)) {
            uart_printf("ERROR: MAC read failed (checksum mismatch or comms error)\n");
            return;
        }

        uart_printf("Response (%d bytes):\n", len);
        for (uint8_t i = 0u; i < len; i++) {
            uart_printf("  [%02d] 0x%02X (%3d)\n", i, data[i], data[i]);
        }

        if (len >= 2u) {
            uint16_t as_u16 = (uint16_t)(data[0] | ((uint16_t)data[1] << 8u));
            uart_printf("  => as uint16 (LE): 0x%04X (%d)\n", as_u16, as_u16);
        }
    }

    else if (strcmp(sub, "fet") == 0) {
        uint16_t fetOptions = 0u;
        if (!BQ27Z746_GetFETOptions(I2C_0_INST, &fetOptions)) {
            uart_printf("ERROR: Failed to read FET Options\n");
            return;
        }
        uart_printf("FET Options (0x45C0): 0x%04X\n", fetOptions);
        uart_printf("  UTFET : %d\n", (fetOptions & (1u << 1)) != 0u);
    }

    else if (strcmp(sub, "utfet") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: gauge utfet <0|1>\n");
            return;
        }
        bool enable = (atoi(tokens[1]) != 0);
        if (!BQ27Z746_SetUTFET_Direct(I2C_0_INST, enable)) {
            uart_printf("ERROR: Failed to write UTFET\n");
            return;
        }
        uart_printf("UTFET %s\n", enable ? "ENABLED" : "DISABLED");

        uint16_t verify = 0u;
        if (BQ27Z746_GetFETOptions(I2C_0_INST, &verify))
            uart_printf("FET Options now: 0x%04X\n", verify);
    }

    else if (strcmp(sub, "monitor") == 0) {
        gauge_monitor_active = true;
        uart_printf("Gauge monitor started — type any command to stop\n");
    }

    else if (strcmp(sub, "stop") == 0) {
        gauge_monitor_active = false;
        uart_printf("Gauge monitor stopped\n");
    }

    else if (strcmp(sub, "reset") == 0) {
        uart_printf("Resetting BQ27Z746...\n");
        if (!BQ27Z746_MAC_Send(I2C_0_INST, BQ27Z746_MAC_RESET)) {
            uart_printf("ERROR: Reset command failed\n");
            return;
        }
        uart_printf("Reset sent\n");
    }

    /* ----------------------------------------------------------
     * Security debug commands
     * ---------------------------------------------------------- */
    else if (strcmp(sub, "security") == 0) {
        uint8_t mode = BQ27Z746_GetSecurityMode(I2C_0_INST);
        const char *label;
        switch (mode) {
            case BQ27Z746_SEC_FULL_ACCESS: label = "FULL ACCESS"; break;
            case BQ27Z746_SEC_UNSEALED:    label = "UNSEALED";    break;
            case BQ27Z746_SEC_SEALED:      label = "SEALED";      break;
            case 0xFFu:                    label = "READ ERROR";  break;
            default:                       label = "RESERVED";    break;
        }
        uart_printf("Security mode: %s (0x%02X)\n", label, mode);
    }

    else if (strcmp(sub, "unseal") == 0) {
        uart_printf("Unsealing...\n");
        if (BQ27Z746_Unseal(I2C_0_INST, BQ27Z746_UNSEAL_KEY1, BQ27Z746_UNSEAL_KEY2))
            uart_printf("OK — device is now unsealed\n");
        else
            uart_printf("ERROR: Unseal failed — wrong keys or comms error\n");
    }

    else if (strcmp(sub, "seal") == 0) {
        uart_printf("Sealing...\n");
        if (BQ27Z746_Seal(I2C_0_INST))
            uart_printf("OK — device is now sealed\n");
        else
            uart_printf("ERROR: Seal failed\n");
    }

    else {
        uart_printf("Unknown gauge sub-command. Type 'gauge' for help.\n");
    }
}



// ─────────────────────────────────────────────
// LED Control Command
// ─────────────────────────────────────────────
 
void cmd_leds(char *args) {
    char *tokens[2];
    int tokenCount = CLI_Tokenize(args, tokens, 2);
 
    if (tokenCount == 0) {
        uart_printf("LED Control CLI:\n"
                    "  led on <1|0>       - Enables and disables boost\n"
                    "  led init              - initialise LED control, both outputs zeroed\n"
                    "  led voltage <mV>      - set boost converter voltage (3490 - 11330 mV)\n"
                    "  led current <mA>      - set LED current (0 -2000 mA)\n"
                    "  led off               - safe shutdown, zeros current then voltage\n"
                    "  led flash <ms>          - start flashing, on-time 1-50 ms\n"
                    "  led flash stop          - stop flashing\n"
                    );
                    
        return;
    }
 
    char *sub = tokens[0];

    if (strcmp(sub, "on") == 0) {
    if (tokenCount < 2) {
        uart_printf("Usage: boost on <1|0>\n");
        return;
    }
        int state = atoi(tokens[1]);
        if (state) {
            enable_led_boost();
            uart_printf("Boost Enabled\n");
        } else {
            disable_led_boost();
            uart_printf("Boost disabled\n");
        }
    }
    // Initialise LED control state and zero both PWM outputs
    else if (strcmp(sub, "init") == 0) {
        LED_control_init();
        uart_printf("LED control initialised. Voltage: 0 mV, Current: 0 mA\n");
    }
 
    // Set boost converter output voltage
    else if (strcmp(sub, "voltage") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: led voltage <mV>  (valid range: 3490 - 11330)\n");
            return;
        }
        uint16_t voltage = (uint16_t)atoi(tokens[1]);
        LED_set_voltage(voltage);
        uart_printf("LED voltage set to %d mV (applied: %d mV)\n", voltage, LED_get_voltage());
    }
 
    // Set LED output current
    else if (strcmp(sub, "current") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: led current <mA>  (valid range: 0 - 2000)\n");
            return;
        }
        uint16_t current = (uint16_t)atoi(tokens[1]);
        LED_set_current(current);
        uart_printf("LED current set to %d mA (applied: %d mA)\n", current, LED_get_current());
    }
 
    // Safe shutdown — zero current first then voltage
    else if (strcmp(sub, "off") == 0) {
        LED_set_current(0);
        // LED_set_voltage(0);
        disable_led_boost();
        uart_printf("LED off. Current zeroed then voltage zeroed.\n");
    }
    else if (strcmp(sub, "flash") == 0) {
    if (tokenCount < 2) {
        uart_printf("Usage: led flash <ms>   (1 - 50 ms)\n"
                    "       led flash stop\n");
        return;
    }
    if (strcmp(tokens[1], "stop") == 0) {
        LED_flash_stop();
        uart_printf("Flash stopped\n");
    } else {
        uint16_t on_ms = (uint16_t)atoi(tokens[1]);
        LED_flash_start(on_ms);
        uart_printf("Flashing: on-time %d ms (ticks: %d)\n", on_ms, on_ms * 500);
    }
    }
    else {
        uart_printf("Unknown led sub-command. Type 'led' for help.\n");
    }
}
 

 // ─────────────────────────────────────────────
// PIR Monitor Command
// ─────────────────────────────────────────────
 
// ─────────────────────────────────────────────
// PIR Monitor Command
// ─────────────────────────────────────────────

static void pir_print_help(void) {
    uart_printf("Usage:\n");
    uart_printf("  pir init <bus> <addr> <type> <step> <threshold>\n");
    uart_printf("     bus       : 0 or 1\n");
    uart_printf("     addr      : 10-bit I2C address (e.g. 0x301)\n");
    uart_printf("     type      : A B C D DIRECT\n");
    uart_printf("     step      : 1 2 3\n");
    uart_printf("     threshold : 0-255 (actual = value * 8 ADC counts)\n");
    uart_printf("  pir status\n");
    uart_printf("  pir monitor\n");
    uart_printf("  pir reset\n");
}

static ZDP323B_FilterType parse_filter_type(const char *s) {
    if      (strcmp(s, "A")      == 0) return ZDP323B_FILTER_TYPE_A;
    else if (strcmp(s, "B")      == 0) return ZDP323B_FILTER_TYPE_B;
    else if (strcmp(s, "C")      == 0) return ZDP323B_FILTER_TYPE_C;
    else if (strcmp(s, "D")      == 0) return ZDP323B_FILTER_TYPE_D;
    else if (strcmp(s, "DIRECT") == 0) return ZDP323B_FILTER_TYPE_DIRECT;
    else                               return ZDP323B_FILTER_TYPE_B; // safe default
}

static ZDP323B_FilterStep parse_filter_step(int s) {
    if      (s == 1) return ZDP323B_FILTER_STEP_1;
    else if (s == 3) return ZDP323B_FILTER_STEP_3;
    else             return ZDP323B_FILTER_STEP_2; // default step 2
}

static const char* filter_type_str(ZDP323B_FilterType t) {
    switch (t) {
        case ZDP323B_FILTER_TYPE_A:      return "A";
        case ZDP323B_FILTER_TYPE_B:      return "B";
        case ZDP323B_FILTER_TYPE_C:      return "C";
        case ZDP323B_FILTER_TYPE_D:      return "D";
        case ZDP323B_FILTER_TYPE_DIRECT: return "DIRECT";
        default:                         return "?";
    }
}

static const char* filter_step_str(ZDP323B_FilterStep s) {
    switch (s) {
        case ZDP323B_FILTER_STEP_1: return "1";
        case ZDP323B_FILTER_STEP_2: return "2";
        case ZDP323B_FILTER_STEP_3: return "3";
        default:                    return "?";
    }
}

// ─────────────────────────────────────────────
// PIR Command
// ─────────────────────────────────────────────

void cmd_pir(char *args) {
    char *tokens[6];
    int tokenCount = CLI_Tokenize(args, tokens, 6);

    if (tokenCount == 0) {
        pir_print_help();
        return;
    }

    char *sub = tokens[0];

    // ── pir init <bus> <addr> <type> <step> <threshold> ──
    if (strcmp(sub, "init") == 0) {
        if (tokenCount < 6) {
            uart_printf("[PIR] init requires: bus addr type step threshold\n");
            pir_print_help();
            return;
        }

        int      busNum   = atoi(tokens[1]);
        uint16_t dev_addr = (uint16_t)strtol(tokens[2], NULL, 0);
        ZDP323B_FilterType ftype = parse_filter_type(tokens[3]);
        ZDP323B_FilterStep fstep = parse_filter_step(atoi(tokens[4]));
        uint8_t  threshold = (uint8_t)atoi(tokens[5]);

        I2C_Regs *targetBus;
        if      (busNum == 0) targetBus = I2C_0_INST;
        else if (busNum == 1) targetBus = I2C_1_INST;
        else {
            uart_printf("[PIR] Invalid bus. Use 0 or 1.\n");
            return;
        }

        uart_printf("[PIR] Initializing on bus %d addr 0x%03X\n", busNum, dev_addr);
        uart_printf("[PIR] Filter: Type %s  Step %s  Threshold: %d (%d ADC counts)\n",
                    filter_type_str(ftype),
                    filter_step_str(fstep),
                    threshold,
                    threshold * 8);

        I2C_Status st = ZDP323B_Init(targetBus, dev_addr, fstep, ftype, threshold);
        if (st != I2C_SUCCESS) {
            uart_printf("[PIR] Init failed with status %d\n", st);
        }
    }

    // ── pir status ────────────────────────────────
    else if (strcmp(sub, "status") == 0) {
        if (!gPIR.initialized) {
            uart_printf("[PIR] Not initialized. Run 'pir init' first.\n");
            return;
        }

        int16_t peak = 0;
        I2C_Status st = ZDP323B_ReadPeakHold(gPIR.i2c, gPIR.dev_addr, &peak);
        if (st != I2C_SUCCESS) {
            uart_printf("[PIR] Failed to read Peak Hold\n");
            return;
        }

        uart_printf("[PIR] Status:\n");
        uart_printf("  Addr       : 0x%03X\n", gPIR.dev_addr);
        uart_printf("  Filter     : Type %s  Step %s\n",
                    filter_type_str(gPIR.armed_cfg.filter_type),
                    filter_step_str(gPIR.armed_cfg.filter_step));
        uart_printf("  Threshold  : %d (%d ADC counts)\n",
                    gPIR.armed_cfg.threshold,
                    gPIR.armed_cfg.threshold * 8);
        uart_printf("  Peak Hold  : %d\n", peak);
        uart_printf("  Motion Flag: %s\n", gPIR.motion_detected ? "SET" : "clear");
        uart_printf("  Monitor    : %s\n", pir_monitor_active   ? "running" : "stopped");
    }

    // ── pir monitor ───────────────────────────────
    else if (strcmp(sub, "monitor") == 0) {
        if (!gPIR.initialized) {
            uart_printf("[PIR] Not initialized. Run 'pir init' first.\n");
            return;
        }

        pir_monitor_active = true;
        uart_printf("[PIR] Monitor started. Send any key to stop.\n");
        uart_printf("%-10s %-10s %-12s\n", "Peak Hold", "Motion",  "Threshold");
        uart_printf("%-10s %-10s %-12s\n", "---------", "------", "---------");
    }

    // ── pir reset ─────────────────────────────────
    else if (strcmp(sub, "reset") == 0) {
        if (!gPIR.initialized) {
            uart_printf("[PIR] Not initialized.\n");
            return;
        }

        // Stop monitor if running
        pir_monitor_active = false;

        // Write default values per datasheet section 9.4 / 13
        ZDP323B_Config reset_cfg = {
            .threshold   = 0x38,
            .trigger_en  = false,
            .filter_step = ZDP323B_FILTER_STEP_2,
            .filter_type = ZDP323B_FILTER_TYPE_B,
        };

        uint8_t config_bytes[7];
        ZDP323B_BuildConfigBytes(&reset_cfg, config_bytes);
        I2C_Status st = ZDP323B_WriteConfig(gPIR.i2c, gPIR.dev_addr, config_bytes);
        if (st != I2C_SUCCESS) {
            uart_printf("[PIR] Reset write failed\n");
            return;
        }

        gPIR.motion_detected = false;
        gPIR.initialized     = false;

        uart_printf("[PIR] Reset complete. Re-run 'pir init' to use.\n");
    }

    else {
        uart_printf("[PIR] Unknown sub-command '%s'\n", sub);
        pir_print_help();
    }
}
 
void cmd_i2cscan10(char *args) {
    char *tokens[1];
    int tokenCount = CLI_Tokenize(args, tokens, 1);

    I2C_Regs *targetBus;
    int busNum = (tokenCount > 0) ? atoi(tokens[0]) : 0;

    if (busNum == 0)      targetBus = I2C_0_INST;
    else if (busNum == 1) targetBus = I2C_1_INST;
    else {
        uart_printf("Invalid bus. Use 0 or 1.\n");
        return;
    }

    uart_printf("Scanning I2C Bus %d (10-bit)...\n", busNum);
    uint8_t foundCount = 0;

    for (uint16_t addr = 0x000; addr <= 0x3FF; addr++) {
        if (I2C_TryAddress10(targetBus, addr)) {
            uart_printf("  Found device at 0x%03X\n", addr);
            foundCount++;
        }
    }

    if (foundCount == 0) {
        uart_printf("No devices found.\n");
    } else {
        uart_printf("Scan complete. %d device(s) found.\n", foundCount);
    }
}

// ─────────────────────────────────────────────
// LTR-329ALS-01 CLI Command
// ─────────────────────────────────────────────

void cmd_ltr(char *args) {
    char *tokens[3];
    int tokenCount = CLI_Tokenize(args, tokens, 3);
    extern volatile bool ltr_monitor_active;

    if (tokenCount == 0) {
        uart_printf("LTR-329ALS-01 CLI:\n"
                    "  ltr init <bus>      - Initialize on I2C bus 0 or 1\n"
                    "  ltr read            - One-shot CH0, CH1 and Lux read\n"
                    "  ltr gain <val>      - Set gain: 1, 2, 4, 8, 48, 96\n"
                    "  ltr monitor         - 200ms live telemetry\n"
                    "  ltr stop            - Stop monitor\n");
        return;
    }

    char *sub = tokens[0];

    if (strcmp(sub, "init") == 0) {
        int busNum = (tokenCount > 1) ? atoi(tokens[1]) : 0;
        I2C_Regs *bus = (busNum == 1) ? I2C_1_INST : I2C_0_INST;
        
        if (LTR329_Init(bus)) {
            uart_printf("LTR-329 initialized successfully on I2C%d\n", busNum);
        } else {
            uart_printf("ERROR: LTR-329 initialization failed\n");
        }
    }
    else if (strcmp(sub, "read") == 0) {
        uint16_t ch0, ch1;
        if (LTR329_ReadData(&ch0, &ch1)) {
            float lux = LTR329_CalculateLux(ch0, ch1);
            uart_printf("CH0: %u  CH1: %u  Lux: %.2f\n", ch0, ch1, lux);
        } else {
            uart_printf("ERROR: Failed to read data\n");
        }
    }
    else if (strcmp(sub, "gain") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: ltr gain <1|2|4|8|48|96>\n");
            return;
        }
        int gainVal = atoi(tokens[1]);
        LTR329_Gain gain;
        switch(gainVal) {
            case 1:  gain = LTR329_GAIN_1X;  break;
            case 2:  gain = LTR329_GAIN_2X;  break;
            case 4:  gain = LTR329_GAIN_4X;  break;
            case 8:  gain = LTR329_GAIN_8X;  break;
            case 48: gain = LTR329_GAIN_48X; break;
            case 96: gain = LTR329_GAIN_96X; break;
            default: uart_printf("Invalid gain value\n"); return;
        }
        if (LTR329_SetGain(gain)) {
            uart_printf("Gain set to %dX\n", gainVal);
        } else {
            uart_printf("ERROR: Failed to set gain\n");
        }
    }
    else if (strcmp(sub, "monitor") == 0) {
        ltr_monitor_active = true;
        uart_printf("LTR monitor started — type any command to stop\n");
    }
    else if (strcmp(sub, "stop") == 0) {
        ltr_monitor_active = false;
        uart_printf("LTR monitor stopped\n");
    }
    else {
        uart_printf("Unknown ltr sub-command\n");
    }
}

void cmd_lis(char *args) {
    char *tokens[3];
    int tokenCount = CLI_Tokenize(args, tokens, 3);
    extern volatile bool lis_monitor_active;

    if (tokenCount == 0) {
        uart_printf("LIS3DH CLI:\n"
                    "  lis init <bus>      - Initialize on I2C bus 0 or 1\n"
                    "  lis read            - One-shot X, Y, Z (mg) read\n"
                    "  lis range <2|4|8|16>- Set full-scale range\n"
                    "  lis monitor         - 200ms live telemetry\n"
                    "  lis stop            - Stop monitor\n");
        return;
    }

    char *sub = tokens[0];

    if (strcmp(sub, "init") == 0) {
        int busNum = (tokenCount > 1) ? atoi(tokens[1]) : 0;
        I2C_Regs *bus = (busNum == 1) ? I2C_1_INST : I2C_0_INST;
        
        // Default to address 0x18
        if (LIS3DH_Init(bus, LIS3DH_I2C_ADDR_0)) {
            uart_printf("LIS3DH initialized successfully on I2C%d (addr 0x%02X)\n", busNum, LIS3DH_I2C_ADDR_0);
        } else {
            uart_printf("ERROR: LIS3DH initialization failed\n");
        }
    }
    else if (strcmp(sub, "read") == 0) {
        float x, y, z;
        if (LIS3DH_ReadMg(&x, &y, &z)) {
            uart_printf("X: %8.2f  Y: %8.2f  Z: %8.2f mg\n", x, y, z);
        } else {
            uart_printf("ERROR: Failed to read data\n");
        }
    }
    else if (strcmp(sub, "range") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: lis range <2|4|8|16>\n");
            return;
        }
        int rVal = atoi(tokens[1]);
        LIS3DH_Range range;
        switch(rVal) {
            case 2:  range = LIS3DH_RANGE_2G;  break;
            case 4:  range = LIS3DH_RANGE_4G;  break;
            case 8:  range = LIS3DH_RANGE_8G;  break;
            case 16: range = LIS3DH_RANGE_16G; break;
            default: uart_printf("Invalid range. Use 2, 4, 8, or 16.\n"); return;
        }
        if (LIS3DH_SetRange(range)) {
            uart_printf("Range set to ±%dg\n", rVal);
        } else {
            uart_printf("ERROR: Failed to set range\n");
        }
    }
    else if (strcmp(sub, "monitor") == 0) {
        lis_monitor_active = true;
        uart_printf("LIS monitor started — type any command to stop\n");
    }
    else if (strcmp(sub, "stop") == 0) {
        lis_monitor_active = false;
        uart_printf("LIS monitor stopped\n");
    }
    else {
        uart_printf("Unknown lis sub-command\n");
    }
}

void cmd_lora(char *args) {
    char *tokens[3];
    int tokenCount = CLI_Tokenize(args, tokens, 3);

    if (tokenCount < 1) {
        uart_printf("Usage: lora <init|read|write|test>\n");
        return;
    }

    char *sub = tokens[0];

    if (strcmp(sub, "init") == 0) {
        // Make sure LoRa power domain is ON (LORA_PON is PB12)
        DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_PON_PIN);
        SX126xIoInit();
        SX126xReset();
        SX126xWaitOnBusy();
        uart_printf("RA-01SH-P LoRa IO initialized and Reset cycle completed.\n");
    }
    else if (strcmp(sub, "read") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: lora read <reg_addr_hex> (e.g. lora read 08E7)\n");
            return;
        }
        uint16_t addr = (uint16_t)strtol(tokens[1], NULL, 16);
        uint8_t val = SX126xReadRegister(addr);
        uart_printf("LoRa Reg [0x%04X] = 0x%02X\n", addr, val);
    }
    else if (strcmp(sub, "write") == 0) {
        if (tokenCount < 3) {
            uart_printf("Usage: lora write <reg_addr_hex> <val_hex> (e.g. lora write 08E7 18)\n");
            return;
        }
        uint16_t addr = (uint16_t)strtol(tokens[1], NULL, 16);
        uint8_t val = (uint8_t)strtol(tokens[2], NULL, 16);
        SX126xWriteRegister(addr, val);
        uart_printf("LoRa Reg [0x%04X] written with 0x%02X\n", addr, val);
    }
    else if (strcmp(sub, "test") == 0) {
        // Read OCP register (default: 0x18 or similar depending on mode)
        uint8_t ocp = SX126xReadRegister(0x08E7);
        uart_printf("OCP Register (0x08E7) Read Value: 0x%02X\n", ocp);
        if (ocp != 0x00 && ocp != 0xFF) {
            uart_printf("LoRa SPI Test: SUCCESS\n");
        } else {
            uart_printf("LoRa SPI Test: FAILED (Check connection or power)\n");
        }
    }
    else {
        uart_printf("Unknown lora sub-command\n");
    }
}

void cmd_imx(char *args) {
    char *tokens[3];
    int tokenCount = CLI_Tokenize(args, tokens, 3);

    if (tokenCount < 1) {
        uart_printf("IMX335 Camera Control CLI:\n"
                    "  imx scan             - Scan I2C1 for the camera\n"
                    "  imx init             - Initialize the camera and start streaming\n"
                    "  imx id               - Read camera sensor ID\n"
                    "  imx read <reg_hex>   - Read a 16-bit register (hex address)\n"
                    "  imx write <reg_hex> <val_hex> - Write a value to a 16-bit register\n"
                    "  imx gain <mdB>       - Set gain in mdB (0 to 72000, e.g. 20000 for 20dB)\n"
                    "  imx exposure <us>    - Set exposure in microseconds (0 to 33266)\n"
                    "  imx tpg <mode>       - Set test pattern (-1:off, 10:H-bars, 11:V-bars)\n");
        return;
    }

    char *sub = tokens[0];

    if (strcmp(sub, "scan") == 0) {
        uart_printf("Scanning for IMX335 on I2C1...\n");
        if (IMX335_Scan()) {
            uart_printf("IMX335 camera found at 0x%02X\n", gIMX335.dev_addr);
        } else {
            uart_printf("IMX335 camera not found (checked 0x1A and 0x36)\n");
        }
    }
    else if (strcmp(sub, "init") == 0) {
        uart_printf("Initializing IMX335 on I2C1...\n");
        if (IMX335_Scan() == false) {
            uart_printf("ERROR: Camera not detected on I2C1\n");
            return;
        }
        if (IMX335_Init(I2C_1_INST)) {
            uart_printf("IMX335 initialized successfully. Streaming started.\n");
        } else {
            uart_printf("ERROR: IMX335 initialization failed\n");
        }
    }
    else if (strcmp(sub, "id") == 0) {
        uint32_t id = 0;
        if (IMX335_ReadID(&id)) {
            uart_printf("IMX335 Sensor ID: 0x%02X (Expected: 0x%02X)\n", id, IMX335_CHIP_ID);
        } else {
            uart_printf("ERROR: Failed to read Sensor ID\n");
        }
    }
    else if (strcmp(sub, "read") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: imx read <reg_hex>\n");
            return;
        }
        uint16_t reg = (uint16_t)strtol(tokens[1], NULL, 16);
        uint8_t val = 0;
        if (IMX335_ReadReg(reg, &val)) {
            uart_printf("Reg [0x%04X] = 0x%02X\n", reg, val);
        } else {
            uart_printf("ERROR: Failed to read register 0x%04X\n", reg);
        }
    }
    else if (strcmp(sub, "write") == 0) {
        if (tokenCount < 3) {
            uart_printf("Usage: imx write <reg_hex> <val_hex>\n");
            return;
        }
        uint16_t reg = (uint16_t)strtol(tokens[1], NULL, 16);
        uint8_t val = (uint8_t)strtol(tokens[2], NULL, 16);
        if (IMX335_WriteReg(reg, val)) {
            uart_printf("Reg [0x%04X] written with 0x%02X\n", reg, val);
        } else {
            uart_printf("ERROR: Failed to write register 0x%04X\n", reg);
        }
    }
    else if (strcmp(sub, "gain") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: imx gain <mdB>\n");
            return;
        }
        uint32_t gain = (uint32_t)atoi(tokens[1]);
        if (IMX335_SetGain(gain)) {
            uart_printf("IMX335 gain set to %u mdB\n", gain);
        } else {
            uart_printf("ERROR: Failed to set gain to %u mdB\n", gain);
        }
    }
    else if (strcmp(sub, "exposure") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: imx exposure <us>\n");
            return;
        }
        uint32_t exp = (uint32_t)atoi(tokens[1]);
        if (IMX335_SetExposure(exp)) {
            uart_printf("IMX335 exposure set to %u us\n", exp);
        } else {
            uart_printf("ERROR: Failed to set exposure to %u us\n", exp);
        }
    }
    else if (strcmp(sub, "tpg") == 0) {
        if (tokenCount < 2) {
            uart_printf("Usage: imx tpg <mode>\n");
            return;
        }
        int32_t mode = (int32_t)atoi(tokens[1]);
        if (IMX335_SetTestPattern(mode)) {
            if (mode >= 0) {
                uart_printf("IMX335 Test Pattern Generator enabled (mode %d)\n", mode);
            } else {
                uart_printf("IMX335 Test Pattern Generator disabled\n");
            }
        } else {
            uart_printf("ERROR: Failed to configure Test Pattern Generator\n");
        }
    }
    else {
        uart_printf("Unknown imx sub-command. Type 'imx' for help.\n");
    }
}

