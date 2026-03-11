#ifndef SPI_MEMORY_H
#define SPI_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"


/* ---------------------------------------------------------------------------
 * Transfer Mode
 * TX_ONLY  : Only TX DMA channel is armed. Used for WREN, WRDI, WRITE.
 * FULL_DUPLEX : Both TX and RX DMA channels are armed. Used for READ, RDID, RDSR.
 * --------------------------------------------------------------------------- */
typedef enum {
    SPI_MEM_MODE_TX_ONLY     = 0,
    SPI_MEM_MODE_FULL_DUPLEX = 1
} SPI_Mem_Mode;

/* ---------------------------------------------------------------------------
 * Transfer Status
 * Replaces the three separate bool flags.
 * The ISR drives this field through BUSY -> DONE or BUSY -> ERROR.
 * Callers only need to watch one field.
 * --------------------------------------------------------------------------- */
typedef enum {
    SPI_MEM_STATUS_IDLE  = 0,
    SPI_MEM_STATUS_BUSY  = 1,
    SPI_MEM_STATUS_DONE  = 2,
    SPI_MEM_STATUS_ERROR = 3
} SPI_Mem_Status;

/* ---------------------------------------------------------------------------
 * Timeout
 * Maximum spin iterations in SPI_Memory_Wait before returning an error.
 * Tune this to your system clock / worst-case transfer length.
 * --------------------------------------------------------------------------- */
#define SPI_MEM_TIMEOUT_COUNT (100000U)
#define SPI_MEM_PACKET_SIZE (16)

/* ---------------------------------------------------------------------------
 * SPI Memory Handle
 *
 * Holds everything needed to manage one SPI slave device:
 *   - Hardware references (SPI peripheral, DMA controller, channel IDs)
 *   - CS GPIO (port + pin) for manual chip-select control
 *   - Per-transfer state (buffers, size, mode, status)
 *
 * One handle per physical device on the bus.
 * --------------------------------------------------------------------------- */
typedef struct {
    /* --- Hardware --- */
    SPI_Regs   *spiInst;        /* SPI peripheral instance                  */
    DMA_Regs   *dmaInst;        /* DMA controller instance                  */
    uint8_t     txDmaCh;        /* DMA channel ID for TX (mem -> SPI)       */
    uint8_t     rxDmaCh;        /* DMA channel ID for RX (SPI -> mem)       */

    /* --- Chip Select GPIO --- */
    GPIO_Regs  *csPort;         /* GPIO port that owns the CS pin           */
    uint32_t    csPin;          /* DL_GPIO_PIN_xx constant for CS           */

    /* --- Per-transfer state (set by SPI_Memory_Arm each call) --- */
    uint8_t    *txBuf;          /* Pointer to outgoing data                 */
    uint8_t    *rxBuf;          /* Pointer to buffer for incoming data      */
    uint16_t    size;           /* Number of bytes in this transfer         */
    SPI_Mem_Mode mode;          /* TX_ONLY or FULL_DUPLEX for this transfer */

    /* --- Status (written by ISR, read by SPI_Memory_Wait) --- */
    volatile SPI_Mem_Status status;

} SPI_Memory_Handle;

/* ---------------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------------- */

/**
 * @brief  One-time initialisation of the handle and its hardware references.
 *
 * @param  handle   Pointer to the handle to initialise.
 * @param  spi      SPI peripheral instance (e.g. SPI_0_INST).
 * @param  txCh     DMA channel ID for TX.
 * @param  rxCh     DMA channel ID for RX.
 * @param  csPort   GPIO port for chip-select (e.g. GPIOA).
 * @param  csPin    GPIO pin for chip-select  (e.g. DL_GPIO_PIN_18).
 */
void SPI_Memory_Init(SPI_Memory_Handle *handle,
                     SPI_Regs          *spi,
                     uint8_t            txCh,
                     uint8_t            rxCh,
                     GPIO_Regs         *csPort,
                     uint32_t           csPin);

/**
 * @brief  Assert chip select (pull CS LOW).
 *
 * @param  handle   Pointer to an initialised handle.
 */
void SPI_Memory_CS_Assert(SPI_Memory_Handle *handle);

/**
 * @brief  Deassert chip select (pull CS HIGH).
 *
 * @param  handle   Pointer to an initialised handle.
 */
void SPI_Memory_CS_Deassert(SPI_Memory_Handle *handle);

/**
 * @brief  Configure DMA and start a transfer.
 *
 *         Buffers and size are supplied here, not at init time, so that each
 *         call is fully self-contained. The handle stores them for the ISR.
 *
 * @param  handle   Pointer to an initialised handle.
 * @param  txBuf    Source buffer (command + data to send).
 * @param  rxBuf    Destination buffer (only meaningful in FULL_DUPLEX mode).
 *                  Pass NULL for TX_ONLY transfers.
 * @param  size     Number of bytes to transfer.
 * @param  mode     SPI_MEM_MODE_TX_ONLY or SPI_MEM_MODE_FULL_DUPLEX.
 */
void SPI_Memory_Arm(SPI_Memory_Handle *handle,
                    uint8_t           *txBuf,
                    uint8_t           *rxBuf,
                    uint16_t           size,
                    SPI_Mem_Mode       mode);

/**
 * @brief  Block until the current transfer completes or timeout expires.
 *
 * @param  handle   Pointer to an initialised handle.
 * @return true  if the transfer completed successfully.
 * @return false if a timeout or error occurred.
 */
bool SPI_Memory_Wait(SPI_Memory_Handle *handle);

/**
 * @brief  ISR body — call this from your SPI peripheral IRQ handler.
 *
 *         Separated from the vector name so the same logic works regardless
 *         of which SPI instance the handle is attached to.
 *
 * @param  handle   Pointer to the handle whose SPI ISR fired.
 */
void SPI_Memory_IRQHandler(SPI_Memory_Handle *handle);

extern SPI_Memory_Handle framSpi;

#endif /* SPI_MEMORY_H */