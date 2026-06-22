#ifndef SX126X_MSPM0_H_
#define SX126X_MSPM0_H_

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// Default LoRa configurations for testing
#define LORA_FRE                                    868000000U  // 868 MHz
#define LORA_TX_OUTPUT_POWER                        14          // 14 dBm
#define LORA_BANDWIDTH                              0           // [0: 125 kHz, 1: 250 kHz, 2: 500 kHz]
#define LORA_SPREADING_FACTOR                       7           // SF7
#define LORA_CODINGRATE                             1           // [1: 4/5, 2: 4/6, 3: 4/7, 4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8
#define LORA_SX126X_SYMBOL_TIMEOUT                  0
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define LORA_RX_TIMEOUT_VALUE                       5000

#endif /* SX126X_MSPM0_H_ */
