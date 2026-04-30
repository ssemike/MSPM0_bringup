#ifndef RAK3172_H
#define RAK3172_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

/* =========================
   RAK3172 Configuration
   ========================= */

#define RAK_UART_INST           MCU_UART_1_INST
#define RAK_UART_IRQN           MCU_UART_1_INST_INT_IRQN
#define RAK_RX_BUFFER_SIZE      256

/* =========================
   Driver Functions
   ========================= */

/**
 * @brief Initialize RAK3172 UART and GPIOs
 */
void RAK3172_Init(void);

/**
 * @brief Power on/off the RAK3172 module
 */
void RAK3172_SetPower(bool on);

/**
 * @brief Reset the RAK3172 module
 */
void RAK3172_Reset(void);

/**
 * @brief Send an AT command to the RAK3172
 * @param cmd The command string (e.g. "AT+VERSION")
 */
void RAK3172_SendCommand(const char *cmd);

/**
 * @brief Wait for a specific response from the RAK3172
 * @param expected The expected response substring (e.g. "OK")
 * @param timeout_ms Maximum time to wait
 * @return true if response received, false otherwise
 */
bool RAK3172_WaitForResponse(const char *expected, uint32_t timeout_ms);

/**
 * @brief Get the last received line from RAK3172
 * @return Pointer to the RX buffer
 */
const char* RAK3172_GetLastResponse(void);

/**
 * @brief UART Interrupt Handler for RAK3172
 * Should be called from the MCU_UART_1_INST_IRQHandler
 */
void RAK3172_UART_Handler(void);

/* =========================
   CLI Command Handler
   ========================= */
void cmd_rak(char *args);

#endif /* RAK3172_H */
