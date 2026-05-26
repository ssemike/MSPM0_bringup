/***************************************************************************//**
 * @file
 * @brief PYD1588 PIR driver header file for MSPM0
 *****************************************************************************/
#ifndef PYD1588_H
#define PYD1588_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// PIR Configuration Bits (25 bits total)
#define PIR_THRESHOLD             ((50 & 0xFF) << 17)
#define PIR_BLIND_TIME            ((0 & 0x0F)  << 13)
#define PIR_PULSE_COUNTER         ((0 & 0x03)  << 11)
#define PIR_WINDOW_TIME           ((0 & 0x03)  << 9)
#define PIR_OPERATION_MODE        ((2 & 0x03)  << 7) // 0: Forced Readout, 1: Interrupt Readout, 2: Wake Up
#define PIR_SOURCE                ((0 & 0x03)  << 5) // 0: BPF (Motion), 1: LPF, 3: Temp
#define PIR_HPF_CUTOFF            ((0 & 0x01)  << 2)
#define PIR_PULSE_DETECTION_MODE  ((1 & 0x01)  << 0)
#define PIR_RESERVED              (0x000010)

#define PIR_INIT_VALUE (PIR_THRESHOLD | PIR_BLIND_TIME | PIR_PULSE_COUNTER | \
                        PIR_WINDOW_TIME | PIR_RESERVED | PIR_OPERATION_MODE | \
                        PIR_SOURCE | PIR_HPF_CUTOFF | PIR_PULSE_DETECTION_MODE)

/**
 * @brief Initialize the PIR sensor by writing the default configuration
 * @return 0 on success, -1 on failure
 */
int PIR_init(void);

/**
 * @brief Configure the PIR sensor with a custom 25-bit configuration and verify it.
 * @param config The 25-bit value to write (in its final intended mode)
 * @return 0 on success, -1 on failure
 */
int PIR_configureAndVerify(uint32_t config);

/**
 * @brief Read PIR data and status/config registers
 * @param statcfg Pointer to store the 25-bit status/config read from sensor
 * @param out_of_range Pointer to store the Out of Range status flag
 * @return 14-bit PIR value (signed if using Bandpass)
 */
int PIR_readData(uint32_t *statcfg, bool *out_of_range);

/**
 * @brief Write a 25-bit configuration value to the PIR sensor via SERIN
 * @param regval The 25-bit value to write
 */
void PIR_writeConfig(uint32_t regval);

/**
 * @brief Clear any pending interrupt on the DL line
 */
void PIR_clearInterrupt(void);

/**
 * @brief Enable or disable temporary Forced Readout mode
 * @param enable true to enable Forced Readout, false to restore intended Wake Up mode
 */
void PIR_enableForcedRead(bool enable);

#endif

