
#include "ti_msp_dl_config.h"
#include "HAL/i2c.h"
#include "functions.h"
#include "HAL/uart.h"
#include "ics/BQ25628/BQ25628_functions.h"
#include "ics/BQ27Z7/BQ27Z7_functions.h"
#include "HAL/spi_master.h"
#include "HAL/spi_mem.h"
#include "ics/ZILOG/ZDP323B.h"
#include "ics/LTR329/LTR329.h"
#include "ics/LIS3DH/LIS3DH.h"
#include "ics/RAK/RAK3172.h"


volatile bool bq_monitor_active    = false;
volatile bool hall_monitor_active  = false;
volatile bool gauge_monitor_active = false;
volatile bool pir_monitor_active   = false;
volatile bool ltr_monitor_active   = false;
volatile bool lis_monitor_active   = false;
volatile uint32_t monitor_rate = 2000; 
volatile uint32_t hall_monitor_counter = 0;

volatile bool g_pir_interrupt_enabled = true;
static bool s_pir_was_enabled_before_i2c = false;

void PIR_interrupt(bool enable);
void PIR_Interrupt_PauseForI2C(void);
void PIR_Interrupt_ResumeAfterI2C(void);
void setupCLI(void) {
    CLI_RegisterCommand("help", cmd_help, "Show available commands");
    CLI_RegisterCommand("pwr",  cmd_pwr,  "Control power rails: 3v8, lora, lte, wifi, stm");
    CLI_RegisterCommand("i2cscan", cmd_i2cscan, "Scan I2C bus: i2cscan <0|1>");
    CLI_RegisterCommand("i2cscan10", cmd_i2cscan10, "Scan 10bit I2C bus: i2cscan10 <0|1>");
    CLI_RegisterCommand("hall", cmd_hall, "Hall sensor: hall <pwr|status>");
    CLI_RegisterCommand("bq", cmd_bq, "BQ25628E charger control - type bq for full help");
    CLI_RegisterCommand("spi", cmd_spi, "SPI Master tx_view, tx_write, test");
    CLI_RegisterCommand("gauge",   cmd_gauge,   "BQ27Z746 gauge — type gauge for help");
    CLI_RegisterCommand("fram", cmd_fram, "MB85RS2MTA FRAM - type fram for help");
    CLI_RegisterCommand("led",    cmd_leds,    "LED control - type led for full help");
    CLI_RegisterCommand("pir",     cmd_pir,     "PIR monitor - type pir for full help");
    CLI_RegisterCommand("ltr",     cmd_ltr,     "LTR-329 ALS sensor - type ltr for help");
    CLI_RegisterCommand("lis",     cmd_lis,     "LIS3DH accelerometer - type lis for help");
    CLI_RegisterCommand("rak",     cmd_rak,     "RAK3172 LoRaWAN module CLI");
}



int main(void)
{
    SYSCFG_DL_init(); 
    uart_init();  
    setupCLI();  
    i2c_init();
    NVIC_EnableIRQ(SPI_1_INST_INT_IRQN);
    NVIC_EnableIRQ(SPI_0_INST_INT_IRQN);
    SPI_Controller_Init(&stm32Spi, SPI_1_INST,  DMA_CH0_CHAN_ID, DMA_CH1_CHAN_ID, gSPI_TxPacket, gSPI_RxPacket, SPI_PACKET_SIZE);
    SPI_Memory_Init(&framSpi, SPI_0_INST, DMA_CH2_CHAN_ID, DMA_CH3_CHAN_ID,DIGITAL_OUTPUT_PORTA_PORT,DIGITAL_OUTPUT_PORTA_CHIP_S_FRAM_PIN);
    char processingBuffer[MAX_INPUT_LEN];
    DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_GAUGE_EN_PIN);
    RAK3172_Init();
    while (1) {
        if (data_received) {
            get_UART_buffer(processingBuffer);
            CLI_ProcessInput(processingBuffer);
        }
        if (hall_monitor_active) {
            delay_cycles(monitor_rate * 32000); // Fixed 200ms delay
            
            uint32_t pin_state = DL_GPIO_readPins(EXTERNAL_INTERRUPT_SETUP_INT_PORT, EXTERNAL_INTERRUPT_SETUP_INT_PIN);
            
            if (pin_state) {
                uart_printf("SETUP_INT: HIGH (1)\n");
            } else {
                uart_printf("SETUP_INT: LOW (0)\n");
            }
            
            // Check if user wants to stop
            if (data_received) {
                hall_monitor_active = false;
                get_UART_buffer(processingBuffer);
                uart_printf("Hall monitoring stopped\n");
            }
        }
        if (bq_monitor_active) {
            delay_cycles(monitor_rate * 32000); // 200 ms refresh

            uint32_t charger_int = DL_GPIO_readPins(EXTERNAL_INTERRUPT_CHARGER_INT_PORT, EXTERNAL_INTERRUPT_CHARGER_INT_PIN); 

            BQ25628E_UpdateTelemetry();
            uint8_t stat1 = BQ25628E_ReadReg8(BQ25628E_REG_STAT1);
            uint8_t chg_stat = (stat1 >> 3) & 0x03;

            const char* desc;
            switch (chg_stat) {
                case 0: desc = "Not Charging / Terminated"; break;
                case 1: desc = "Pre/Trickle/Fast (CC)"; break;
                case 2: desc = "Taper (CV)"; break;
                case 3: desc = "Top-Off"; break;
                default: desc = "Unknown";
            }

            uart_printf("=== BQ25628E MONITOR (200ms) ===\n");
            uart_printf("CHARGER_INT : %s\n", charger_int ? "HIGH" : "LOW");
            uart_printf("Charging Status : %s  (CHG_STAT[4:3] = 0b%02b)\n", desc, chg_stat);
            uart_printf("VBUS:%4dmV VBAT:%4dmV  VSYS:%4dmV  IBUS:%4dmA  IBAT:%4dmA\n",BQ25628E_Get_VBUS_mV(),
                        BQ25628E_Get_VBAT_mV(), BQ25628E_Get_VSYS_mV(),
                        BQ25628E_Get_IBUS_mA(), BQ25628E_Get_IBAT_mA());
            uart_printf("ChgFlag0:0x%02X  FaultFlag0:0x%02X\n",
                        BQ25628E_ReadReg8(BQ25628E_REG_CHG_FLAG0),
                        BQ25628E_ReadReg8(BQ25628E_REG_FAULT_FLAG0));

            if (data_received) { 
                bq_monitor_active = false;
                get_UART_buffer(processingBuffer);
                uart_printf("Monitor stopped\n");
            }
        }
        if (gauge_monitor_active) {
            delay_cycles(monitor_rate * 32000);

            BQ27Z746_UpdateTelemetry(I2C_0_INST);

            uint16_t tte = BQ27Z746_Get_TimeToEmpty_min();
            uint16_t ttf = BQ27Z746_Get_TimeToFull_min();

            /* Determine a one-word state string from cached BatteryStatus */
            const char *state;
            if      (BQ27Z746_IsDischarging())     state = "DISCHARGING";
            else if (BQ27Z746_IsFullyCharged())    state = "FULLY CHARGED";
            else if (BQ27Z746_IsFullyDischarged()) state = "FULLY DISCHARGED";
            else                                   state = "CHARGING";

            uart_printf("=== BQ27Z746 MONITOR (200ms) ===\n");
            uart_printf("State  : %s\n", state);
            uart_printf("SOC    : %3d %%   SoH: %3d %%   Cycles: %d\n",
                        BQ27Z746_Get_SOC_pct(),
                        BQ27Z746_Get_StateOfHealth_pct(),
                        BQ27Z746_Get_CycleCount());
            uart_printf("VBAT   : %4d mV\n", BQ27Z746_Get_Voltage_mV());
            uart_printf("IBAT   : %5d mA   AvgI: %5d mA\n",
                        BQ27Z746_Get_Current_mA(),
                        BQ27Z746_Get_AvgCurrent_mA());
            uart_printf("AvgPwr : %5d mW\n", BQ27Z746_Get_AvgPower_mW());
            uart_printf("RemCap : %4d mAh  FullCap: %4d mAh\n",
                        BQ27Z746_Get_RemainingCap_mAh(),
                        BQ27Z746_Get_FullChargeCap_mAh());
            uart_printf("Temp   : %3d C   InternalTemp: %3d C\n",
                        BQ27Z746_Get_Temperature_C(),
                        BQ27Z746_Get_InternalTemp_C());

            /* TTE / TTF with 0xFFFF guard */
            uart_printf("TTE    : ");
            if (tte == 0xFFFFu) uart_printf("  ---");
            else                uart_printf("%4d min", tte);

            uart_printf("   TTF: ");
            if (ttf == 0xFFFFu) uart_printf("  ---\n");
            else                uart_printf("%4d min\n", ttf);

            uart_printf("Status : 0x%04X\n", BQ27Z746_Get_BatteryStatus());
            uart_printf("--------------------------------\n");

            if (data_received) {
                gauge_monitor_active = false;
                get_UART_buffer(processingBuffer);
                uart_printf("Gauge monitor stopped\n");
            }
        }
        if (pir_monitor_active) {

            // Read Peak Hold from sensor
            int16_t peak = 0;
            I2C_Status st = ZDP323B_ReadPeakHold(gPIR.i2c, gPIR.dev_addr, &peak);

            if (st != I2C_SUCCESS) {
                uart_printf("[PIR] Read error during monitor\n");
                pir_monitor_active = false;
            } else {
                // Check and clear motion flag atomically
                bool motion = gPIR.motion_detected;
                if (motion) gPIR.motion_detected = false;

                uart_printf("[PIR] Peak: %5d  Threshold: ±%4d  Motion: %s\n",
                            peak,
                            gPIR.armed_cfg.threshold * 8,
                            motion ? "DETECTED" : "-");
            }
            if (data_received) {
                pir_monitor_active = false;
                get_UART_buffer(processingBuffer);
                uart_printf("[PIR] Monitor stopped\n");
            }
             pir_monitor_active = false;
             PIR_interrupt(true);
        }
        if (ltr_monitor_active) {
            delay_cycles(monitor_rate * 32000); // 200ms

            uint16_t ch0, ch1;
            if (LTR329_ReadData(&ch0, &ch1)) {
                float lux = LTR329_CalculateLux(ch0, ch1);
                uart_printf("[LTR] CH0: %5u  CH1: %5u  Lux: %7.2f\n", ch0, ch1, lux);
            } else {
                uart_printf("[LTR] Read error\n");
                ltr_monitor_active = false;
            }

            if (data_received) {
                ltr_monitor_active = false;
                get_UART_buffer(processingBuffer);
                uart_printf("[LTR] Monitor stopped\n");
            }
        }
        if (lis_monitor_active) {
            delay_cycles(monitor_rate * 32000); // 200ms

            float x, y, z;
            if (LIS3DH_ReadMg(&x, &y, &z)) {
                uart_printf("[LIS] X: %8.2f  Y: %8.2f  Z: %8.2f mg\n", x, y, z);
            } else {
                uart_printf("[LIS] Read error\n");
                lis_monitor_active = false;
            }

            if (data_received) {
                lis_monitor_active = false;
                get_UART_buffer(processingBuffer);
                uart_printf("[LIS] Monitor stopped\n");
            }
        }
    }
}

void UART_0_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_RX:
            UARTReceive();
            break;
        default:
            break;
    }
}

void MCU_UART_1_INST_IRQHandler(void)
{
    RAK3172_UART_Handler();
}


// void FLASH_CONTROL_INST_IRQHandler(void)
// {
//     DL_TimerG_stopCounter(FLASH_CONTROL_INST); 
//     DL_TimerG_clearInterruptStatus(FLASH_CONTROL_INST, DL_TIMER_INTERRUPT_CC1_UP_EVENT);  
// }

// ─────────────────────────────────────────────
// GROUP1 IRQ Handler (Handles GPIOA and GPIOB)
// ─────────────────────────────────────────────

void GROUP1_IRQHandler(void) {
    // Determine which group source triggered the interrupt
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        
        case DL_INTERRUPT_GROUP1_IIDX_GPIOA:
            // Now check which specific pin on Port A triggered it
            switch (DL_GPIO_getPendingInterrupt(GPIOA)) {
                case EXTERNAL_INTERRUPT_PIR_TRIGGER_IIDX:
                    // 50µs pulse from ZDP323B detected
                    pir_monitor_active = true;
                    ZDP323B_MotionISR();
                    PIR_interrupt(false);

                    break;
                default:
                    break;
            }
            break;

        case DL_INTERRUPT_GROUP1_IIDX_GPIOB:
            // Handle Port B (e.g. CHARGER_INT) if needed, or just clear it
            DL_GPIO_getPendingInterrupt(GPIOB);
            break;

        default:
            break;
    }
}
void PIR_interrupt(bool enable) {
    if(enable){
        DL_GPIO_clearInterruptStatus(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        NVIC_ClearPendingIRQ(EXTERNAL_INTERRUPT_GPIOA_INT_IRQN);
        DL_GPIO_enableInterrupt(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        g_pir_interrupt_enabled = true;
    }else{
        DL_GPIO_clearInterruptStatus(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        DL_GPIO_disableInterrupt(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        g_pir_interrupt_enabled = false;
    }
}

void PIR_Interrupt_PauseForI2C(void) {
    s_pir_was_enabled_before_i2c = g_pir_interrupt_enabled;
    if (s_pir_was_enabled_before_i2c) {
        DL_GPIO_disableInterrupt(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    }
}

void PIR_Interrupt_ResumeAfterI2C(void) {
    if (s_pir_was_enabled_before_i2c) {
        delay_cycles(200); // Small settle delay
        DL_GPIO_clearInterruptStatus(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
        NVIC_ClearPendingIRQ(EXTERNAL_INTERRUPT_GPIOA_INT_IRQN);
        DL_GPIO_enableInterrupt(EXTERNAL_INTERRUPT_PIR_TRIGGER_PORT, EXTERNAL_INTERRUPT_PIR_TRIGGER_PIN);
    }
}