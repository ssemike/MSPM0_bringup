#ifndef SPI_PERIPHERAL_H
#define SPI_PERIPHERAL_H
#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"


#define SPI_PACKET_SIZE (16)
/**
 * @brief SPI Peripheral Handle for Modular Use
 */
typedef struct {
    /* Hardware Resources */
    SPI_Regs  *spiInst;        // Pointer to SPI instance
    DMA_Regs  *dmaInst;        // Pointer to DMA controller
    uint8_t    txDmaCh;        // DMA Channel ID for TX
    uint8_t    rxDmaCh;        // DMA Channel ID for RX
    /* Data Buffers */
    uint8_t   *txBuf;
    uint8_t   *rxBuf;
    uint16_t   size;
    /* Status Flags */
    volatile bool transferComplete;
} SPI_Peripheral_Handle;


/**
 * @brief Configures the handle with hardware and buffer info.
 */
void SPI_Peripheral_Init(SPI_Peripheral_Handle *handle, 
                         SPI_Regs *spi, 
                         uint8_t txCh, uint8_t rxCh,
                         uint8_t *txBuf, uint8_t *rxBuf, 
                         uint16_t len);

/**
 * @brief Arms the specific SPI instance provided in the handle.
 */
void SPI_Peripheral_Arm(SPI_Peripheral_Handle *handle);


extern SPI_Peripheral_Handle stm32Spi;
extern uint8_t gSPI_TxPacket[SPI_PACKET_SIZE];
extern uint8_t gSPI_RxPacket[SPI_PACKET_SIZE];


#endif /* SPI_PERIPHERAL_H */