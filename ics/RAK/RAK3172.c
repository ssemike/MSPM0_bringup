#include "RAK3172.h"
#include "HAL/uart.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================
   Static State
   ========================= */

static char rak_rx_buffer[RAK_RX_BUFFER_SIZE];
static volatile uint16_t rak_rx_index = 0;
static volatile bool rak_response_ready = false;

/* =========================
   Driver Implementation
   ========================= */

void RAK3172_Init(void) {
    rak_rx_index = 0;
    rak_response_ready = false;
    memset(rak_rx_buffer, 0, RAK_RX_BUFFER_SIZE);

    // SysConfig enabled both RX and TX — disable TX, we don't use it
    DL_UART_Main_disableInterrupt(MCU_UART_1_INST, DL_UART_MAIN_INTERRUPT_TX);
    DL_UART_Main_enableInterrupt(MCU_UART_1_INST, DL_UART_MAIN_INTERRUPT_RX | DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);

    NVIC_ClearPendingIRQ(MCU_UART_1_INST_INT_IRQN);
    NVIC_EnableIRQ(MCU_UART_1_INST_INT_IRQN);
}





void RAK3172_SetPower(bool on) {
    if (on) {
        DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_PON_PIN);
    } else {
        DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_PON_PIN);
    }
}

void RAK3172_Reset(void) {
    // Reset is active low per usual LoRa module standards, 
    // but check if LORA_1_RST needs to be pulsed.
    DL_GPIO_clearPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_1_RST_PIN);
    delay_cycles(32000 * 100); // 100ms
    DL_GPIO_setPins(DIGITAL_OUTPUT_PORTB_PORT, DIGITAL_OUTPUT_PORTB_LORA_1_RST_PIN);
    delay_cycles(32000 * 2000); // 2000ms for boot
}

void RAK3172_SendCommand(const char *cmd) {
    rak_response_ready = false;
    rak_rx_index = 0;
    memset(rak_rx_buffer, 0, RAK_RX_BUFFER_SIZE);

    while (*cmd) {
        DL_UART_Main_transmitDataBlocking(RAK_UART_INST, *cmd++);
    }
    // All AT commands must end with \r\n
    DL_UART_Main_transmitDataBlocking(RAK_UART_INST, '\r');
    DL_UART_Main_transmitDataBlocking(RAK_UART_INST, '\n');
}

bool RAK3172_WaitForResponse(const char *expected, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (rak_response_ready) {
            if (strstr(rak_rx_buffer, expected) != NULL) {
                return true;
            }
            rak_response_ready = false; // Reset if not what we wanted
            rak_rx_index = 0;
            memset(rak_rx_buffer, 0, RAK_RX_BUFFER_SIZE);
        }
        delay_cycles(32000); // ~1ms
        elapsed++;
    }
    return false;
}

const char* RAK3172_GetLastResponse(void) {
    return rak_rx_buffer;
}
void RAK3172_UART_Handler(void) {
    switch (DL_UART_Main_getPendingInterrupt(RAK_UART_INST)) {
        case DL_UART_MAIN_IIDX_RX:
#ifdef DL_UART_MAIN_IIDX_RX_TIMEOUT
        case DL_UART_MAIN_IIDX_RX_TIMEOUT:
#endif
#ifdef DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR
        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
#endif
#ifdef DL_UART_MAIN_IIDX_OVERRUN_ERROR
        case DL_UART_MAIN_IIDX_OVERRUN_ERROR:
#endif
#ifdef DL_UART_MAIN_IIDX_FRAMING_ERROR
        case DL_UART_MAIN_IIDX_FRAMING_ERROR:
#endif
#ifdef DL_UART_MAIN_IIDX_PARITY_ERROR
        case DL_UART_MAIN_IIDX_PARITY_ERROR:
#endif
        {
            while (DL_UART_Main_isRXFIFOEmpty(RAK_UART_INST) == false) {
                char c = DL_UART_Main_receiveData(RAK_UART_INST);
                if (rak_rx_index < RAK_RX_BUFFER_SIZE - 1) {
                    rak_rx_buffer[rak_rx_index++] = c;
                    if (c == '\n') {
                        rak_rx_buffer[rak_rx_index] = '\0';
                        rak_response_ready = true;
                    }
                } else {
                    rak_rx_index = 0;
                    rak_response_ready = false;
                    memset(rak_rx_buffer, 0, RAK_RX_BUFFER_SIZE);
                }
            }
            break;
        }
        case DL_UART_MAIN_IIDX_TX:
            // Not used — do nothing, interrupt clears itself when FIFO fills
            break;
        default:
            break;
    }
}

/* =========================
   CLI Command Handler
   ========================= */

void cmd_rak(char *args) {
    char *tokens[2];
    int tokenCount = CLI_Tokenize(args, tokens, 2);

    if (tokenCount == 0) {
        uart_printf("RAK3172 CLI Help:\n");
        uart_printf("  rak init        - Initialize UART and GPIOs\n");
        uart_printf("  rak pwr <1|0>   - Set power rail\n");
        uart_printf("  rak reset       - Pulse reset pin\n");
        uart_printf("  rak listen      - Echo raw UART1 traffic (press key to stop)\n");
        uart_printf("  rak cmd <AT...> - Send raw AT command\n");
        return;
    }

    if (strcmp(tokens[0], "init") == 0) {

        RAK3172_Init();
        uart_printf("RAK3172 Driver Initialized (UART1)\n");
    }
    else if (strcmp(tokens[0], "pwr") == 0 && tokenCount >= 2) {
        bool on = atoi(tokens[1]);
        RAK3172_SetPower(on);
        uart_printf("RAK3172 Power: %s\n", on ? "ON" : "OFF");
    }
    else if (strcmp(tokens[0], "reset") == 0) {
        RAK3172_Reset();
        uart_printf("RAK3172 Reset pulsed\n");
    }
    else if (strcmp(tokens[0], "listen") == 0) {
        uart_printf("Listening to RAK3172 (UART1) [Hex mode]... Pulsing reset...\n");
        RAK3172_Reset();
        uart_printf("Reset done. Press any key to stop.\n");
        while (1) {
            if (rak_rx_index > 0) {
                NVIC_DisableIRQ(MCU_UART_1_INST_INT_IRQN);
                uint16_t len = rak_rx_index;
                char temp[RAK_RX_BUFFER_SIZE];
                memcpy(temp, rak_rx_buffer, len);
                rak_rx_index = 0;
                memset(rak_rx_buffer, 0, RAK_RX_BUFFER_SIZE);
                NVIC_EnableIRQ(MCU_UART_1_INST_INT_IRQN);
                
                for(int i = 0; i < len; i++) {
                    char c = temp[i];
                    if (c >= 32 && c <= 126) uart_printf("%c", c);
                    else uart_printf("[%02X]", (uint8_t)c);
                }
            }
            if (DL_UART_Main_isRXFIFOEmpty(UART_0_INST) == false) {
                DL_UART_Main_receiveData(UART_0_INST);
                uart_printf("\nListen stopped.\n");
                break;
            }
            delay_cycles(32000 * 10);
        }
    }
    else if (strcmp(tokens[0], "cmd") == 0 && tokenCount >= 2) {

        char *cmd_ptr = strstr(args, "cmd ");
        if (cmd_ptr) {
            cmd_ptr += 4;
            uart_printf("Sending: %s\n", cmd_ptr);
            RAK3172_SendCommand(cmd_ptr);
            
            // Wait for response with 2s timeout
            if (RAK3172_WaitForResponse("OK", 2000)) {
                uart_printf("Response: %s\n", RAK3172_GetLastResponse());
            } else if (strstr(RAK3172_GetLastResponse(), "ERROR") != NULL) {
                uart_printf("Error: %s\n", RAK3172_GetLastResponse());
            } else {
                uart_printf("Timeout or No Response. Buffer: %s (idx: %d)\n", RAK3172_GetLastResponse(), rak_rx_index);
            }
        }
        else if (strcmp(tokens[0], "cmd") == 0 && tokenCount >= 2) {
            uart_printf("Sending: %s\n", tokens[1]);
            RAK3172_SendCommand(tokens[1]);

            if (RAK3172_WaitForResponse("OK", 2000)) {
                uart_printf("Response: %s\n", RAK3172_GetLastResponse());
            } else if (strstr(RAK3172_GetLastResponse(), "ERROR") != NULL) {
                uart_printf("Error: %s\n", RAK3172_GetLastResponse());
            } else {
                uart_printf("Timeout or No Response. Buffer: %s (idx: %d)\n", RAK3172_GetLastResponse(), rak_rx_index);
            }
        }
    }
    else {
        uart_printf("Unknown RAK command\n");
    }
}
