#include "spi_mem.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------------- */

/**
 * @brief  Arms only the TX DMA channel.
 *         Used for WREN, WRDI, WRITE — transfers where received bytes are
 *         irrelevant and we only care that the shift register drains.
 */
static void arm_tx_only(SPI_Memory_Handle *handle)
{
    DL_DMA_setSrcAddr(handle->dmaInst,
                      handle->txDmaCh,
                      (uint32_t)handle->txBuf);

    DL_DMA_setDestAddr(handle->dmaInst,
                       handle->txDmaCh,
                       (uint32_t)(&handle->spiInst->TXDATA));

    DL_DMA_setTransferSize(handle->dmaInst,
                           handle->txDmaCh,
                           handle->size);

    DL_DMA_enableChannel(handle->dmaInst, handle->txDmaCh);
}

/**
 * @brief  Arms both TX and RX DMA channels.
 *         RX is enabled first so it is ready before the clock starts.
 *         Used for READ, RDID, RDSR — transfers where we need the response.
 */
static void arm_full_duplex(SPI_Memory_Handle *handle)
{
    /* TX side */
    DL_DMA_setSrcAddr(handle->dmaInst,
                      handle->txDmaCh,
                      (uint32_t)handle->txBuf);

    DL_DMA_setDestAddr(handle->dmaInst,
                       handle->txDmaCh,
                       (uint32_t)(&handle->spiInst->TXDATA));

    DL_DMA_setTransferSize(handle->dmaInst,
                           handle->txDmaCh,
                           handle->size);

    /* RX side */
    DL_DMA_setSrcAddr(handle->dmaInst,
                      handle->rxDmaCh,
                      (uint32_t)(&handle->spiInst->RXDATA));

    DL_DMA_setDestAddr(handle->dmaInst,
                       handle->rxDmaCh,
                       (uint32_t)handle->rxBuf);

    DL_DMA_setTransferSize(handle->dmaInst,
                           handle->rxDmaCh,
                           handle->size);

    /* Enable RX before TX so the channel is ready when the clock starts */
    DL_DMA_enableChannel(handle->dmaInst, handle->rxDmaCh);
    DL_DMA_enableChannel(handle->dmaInst, handle->txDmaCh);
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void SPI_Memory_Init(SPI_Memory_Handle *handle,
                     SPI_Regs          *spi,
                     uint8_t            txCh,
                     uint8_t            rxCh,
                     GPIO_Regs         *csPort,
                     uint32_t           csPin)
{
    handle->spiInst  = spi;
    handle->dmaInst  = DMA;
    handle->txDmaCh  = txCh;
    handle->rxDmaCh  = rxCh;
    handle->csPort   = csPort;
    handle->csPin    = csPin;

    /* Clear per-transfer fields */
    handle->txBuf    = NULL;
    handle->rxBuf    = NULL;
    handle->size     = 0;
    handle->mode     = SPI_MEM_MODE_TX_ONLY;
    handle->status   = SPI_MEM_STATUS_IDLE;

    /* Ensure CS starts deasserted */
    DL_GPIO_setPins(handle->csPort, handle->csPin);
}

void SPI_Memory_CS_Assert(SPI_Memory_Handle *handle)
{
    DL_GPIO_clearPins(handle->csPort, handle->csPin);
}

void SPI_Memory_CS_Deassert(SPI_Memory_Handle *handle)
{
    DL_GPIO_setPins(handle->csPort, handle->csPin);
}

void SPI_Memory_Arm(SPI_Memory_Handle *handle,
                    uint8_t           *txBuf,
                    uint8_t           *rxBuf,
                    uint16_t           size,
                    SPI_Mem_Mode       mode)
{
    /* Store transfer parameters in the handle */
    handle->txBuf  = txBuf;
    handle->rxBuf  = rxBuf;
    handle->size   = size;
    handle->mode   = mode;
    handle->status = SPI_MEM_STATUS_BUSY;

    /* Clear RX buffer before arming when receiving */
    if (mode == SPI_MEM_MODE_FULL_DUPLEX && rxBuf != NULL) {
        memset(rxBuf, 0, size);
    }

    if (mode == SPI_MEM_MODE_TX_ONLY) {
        DL_SPI_clearInterruptStatus(handle->spiInst, DL_SPI_INTERRUPT_TX_EMPTY);
        DL_SPI_enableInterrupt(handle->spiInst, DL_SPI_INTERRUPT_TX_EMPTY);
        arm_tx_only(handle);
    }else {
        arm_full_duplex(handle);
    }
}

bool SPI_Memory_Wait(SPI_Memory_Handle *handle)
{
    uint32_t timeout = SPI_MEM_TIMEOUT_COUNT;

    while (handle->status == SPI_MEM_STATUS_BUSY) {
        if (--timeout == 0) {
            handle->status = SPI_MEM_STATUS_ERROR;
            return false;
        }
    }

    return (handle->status == SPI_MEM_STATUS_DONE);
}

void SPI_Memory_IRQHandler(SPI_Memory_Handle *handle)
{
    switch (DL_SPI_getPendingInterrupt(handle->spiInst)) {

    case DL_SPI_IIDX_TX_EMPTY:
        if (handle->mode == SPI_MEM_MODE_TX_ONLY) {
            uint32_t dmaMask   = (1u << handle->txDmaCh);
            uint32_t dmaStatus = DL_DMA_getRawInterruptStatus(DMA, dmaMask);

            if (dmaStatus & dmaMask) {
                handle->status = SPI_MEM_STATUS_DONE;
                DL_DMA_clearInterruptStatus(DMA, dmaMask);
                DL_SPI_disableInterrupt(handle->spiInst, DL_SPI_INTERRUPT_TX_EMPTY);
            }
        }
        break;

    case DL_SPI_IIDX_DMA_DONE_RX:
        if (handle->mode == SPI_MEM_MODE_FULL_DUPLEX) {
            handle->status = SPI_MEM_STATUS_DONE;
        }
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------------------------
 * IRQ vector — delegates to the generic handler.
 * If you have multiple SPI instances, add a second vector that passes its
 * own handle.
 * --------------------------------------------------------------------------- */
void SPI_0_INST_IRQHandler(void)
{
    SPI_Memory_IRQHandler(&framSpi);
}

SPI_Memory_Handle framSpi;