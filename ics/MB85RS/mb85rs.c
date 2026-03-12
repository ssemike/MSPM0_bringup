#include "mb85rs.h"
#include <string.h>

/* ---------------------------------------------------------------------------
 * Internal helpers
 *
 * These functions are not exposed in the header. Each one owns its own
 * command buffer on the stack, asserts CS, arms the DMA, waits, and
 * deasserts CS before returning.  The caller never touches CS or DMA.
 * --------------------------------------------------------------------------- */

/**
 * @brief  Populate the 3 address bytes into a buffer starting at offset 1.
 *
 *         The datasheet specifies a 24-bit address sent MSB first.
 *         The upper 6 bits of the 24-bit field are ignored by the device
 *         (only 18 bits are needed for 256K), but we send all 24 cleanly.
 *
 *         buf[0] is assumed to already hold the op-code.
 *         buf[1..3] receive the address bytes.
 */
static void pack_address(uint8_t *buf, uint32_t address)
{
    buf[1] = (uint8_t)((address >> 16) & 0xFFU);
    buf[2] = (uint8_t)((address >>  8) & 0xFFU);
    buf[3] = (uint8_t)( address        & 0xFFU);
}

/**
 * @brief  Send a single-byte command that carries no address and no data.
 *         Used for WREN, WRDI, and SLEEP.
 *
 * @return MB85RS_OK or MB85RS_ERR_TRANSFER.
 */
static MB85RS_Error send_cmd_only(MB85RS_Handle *fram, uint8_t cmd)
{
    uint8_t tx = cmd;

    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_Arm(fram->spi, &tx, NULL, MB85RS_CMD_ONLY_LEN, SPI_MEM_MODE_TX_ONLY);

    if (!SPI_Memory_Wait(fram->spi)) {
        SPI_Memory_CS_Deassert(fram->spi);
        return MB85RS_ERR_TRANSFER;
    }

    SPI_Memory_CS_Deassert(fram->spi);
    return MB85RS_OK;
}

/**
 * @brief  Issue WREN in its own CS pulse.
 *
 *         Must be called immediately before any write command.
 *         The CS must rise and fall again before the write op-code is sent —
 *         this function provides that complete pulse.
 */
static MB85RS_Error wren(MB85RS_Handle *fram)
{
    return send_cmd_only(fram, MB85RS_CMD_WREN);
}

/**
 * @brief  Issue WRDI in its own CS pulse.
 *
 *         Called after a write sequence to re-protect the device.
 */
static MB85RS_Error wrdi(MB85RS_Handle *fram)
{
    return send_cmd_only(fram, MB85RS_CMD_WRDI);
}

/**
 * @brief  Validate guard conditions common to read and write operations.
 */
static MB85RS_Error check_bounds(const MB85RS_Handle *fram,
                                 uint32_t             address,
                                 uint32_t             len)
{
    if (fram == NULL || fram->spi == NULL) {
        return MB85RS_ERR_PARAM;
    }
    if (!fram->initialised) {
        return MB85RS_ERR_PARAM;
    }
    if (len == 0) {
        return MB85RS_ERR_PARAM;
    }
    if (address > MB85RS_MAX_ADDRESS) {
        return MB85RS_ERR_BOUNDS;
    }
    if ((address + len - 1U) > MB85RS_MAX_ADDRESS) {
        return MB85RS_ERR_BOUNDS;
    }
    return MB85RS_OK;
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

MB85RS_Error MB85RS_Init(MB85RS_Handle *fram, SPI_Memory_Handle *spi)
{
    if (fram == NULL || spi == NULL) {
        return MB85RS_ERR_PARAM;
    }

    fram->spi         = spi;
    fram->sleeping    = false;
    fram->initialised = false;

    /* Verify the device is present and responding correctly */
    MB85RS_Error err = MB85RS_ReadID(fram);
    if (err != MB85RS_OK) {
        return err;
    }

    fram->initialised = true;
    return MB85RS_OK;
}

MB85RS_Error MB85RS_ReadID(MB85RS_Handle *fram)
{
    if (fram == NULL || fram->spi == NULL) {
        return MB85RS_ERR_PARAM;
    }

    /*
     * RDID transaction:
     *   TX: op-code (1 byte), then 4 dummy bytes to clock out the response.
     *   RX: first byte is garbage (clocked in during op-code), then 4 id bytes.
     *
     *   Total bus bytes = 5 (op-code + 4 id bytes).
     *   rxBuf[0] is discarded; rxBuf[1..4] hold the device ID.
     */
    uint8_t tx[MB85RS_RDID_LEN];
    uint8_t rx[MB85RS_RDID_LEN];

    memset(tx, 0x00, sizeof(tx));
    tx[0] = MB85RS_CMD_RDID;

    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_Arm(fram->spi, tx, rx, MB85RS_RDID_LEN, SPI_MEM_MODE_FULL_DUPLEX);

    if (!SPI_Memory_Wait(fram->spi)) {
        SPI_Memory_CS_Deassert(fram->spi);
        return MB85RS_ERR_TRANSFER;
    }

    SPI_Memory_CS_Deassert(fram->spi);

    /*
     * rx[0] was clocked in while we sent the op-code — ignore it.
     * rx[1] = Manufacturer ID   (expected 0x04)
     * rx[2] = Continuation code (expected 0x7F)
     * rx[3] = Product ID byte 1 (expected 0x48)
     * rx[4] = Product ID byte 2 (expected 0x03)
     */
    if (rx[1] != MB85RS_ID_MANUFACTURER  ||
        rx[2] != MB85RS_ID_CONTINUATION  ||
        rx[3] != MB85RS_ID_PRODUCT1      ||
        rx[4] != MB85RS_ID_PRODUCT2) {
        return MB85RS_ERR_DEVICE_ID;
    }

    return MB85RS_OK;
}

MB85RS_Error MB85RS_ReadStatus(MB85RS_Handle *fram, uint8_t *sr)
{
    if (fram == NULL || fram->spi == NULL || sr == NULL) {
        return MB85RS_ERR_PARAM;
    }

    /*
     * RDSR transaction:
     *   TX: op-code + 1 dummy byte  (2 bytes total to clock out the response)
     *   RX: rx[0] garbage, rx[1] = status register value
     */
    uint8_t tx[MB85RS_RDSR_LEN];
    uint8_t rx[MB85RS_RDSR_LEN];

    tx[0] = MB85RS_CMD_RDSR;
    tx[1] = 0x00;

    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_Arm(fram->spi, tx, rx, MB85RS_RDSR_LEN, SPI_MEM_MODE_FULL_DUPLEX);

    if (!SPI_Memory_Wait(fram->spi)) {
        SPI_Memory_CS_Deassert(fram->spi);
        return MB85RS_ERR_TRANSFER;
    }

    SPI_Memory_CS_Deassert(fram->spi);

    /* rx[0] clocked in during op-code — discard. rx[1] is the status byte. */
    *sr = rx[1];
    return MB85RS_OK;
}

MB85RS_Error MB85RS_WriteStatus(MB85RS_Handle *fram, uint8_t value)
{
    if (fram == NULL || fram->spi == NULL) {
        return MB85RS_ERR_PARAM;
    }

    /* WREN must precede WRSR in its own CS pulse */
    MB85RS_Error err = wren(fram);
    if (err != MB85RS_OK) {
        return err;
    }

    /*
     * WRSR transaction:
     *   TX: op-code + 1 data byte
     *   RX: not needed — TX_ONLY
     */
    uint8_t tx[MB85RS_WRSR_LEN];
    tx[0] = MB85RS_CMD_WRSR;
    tx[1] = value;

    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_Arm(fram->spi, tx, NULL, MB85RS_WRSR_LEN, SPI_MEM_MODE_TX_ONLY);

    if (!SPI_Memory_Wait(fram->spi)) {
        SPI_Memory_CS_Deassert(fram->spi);
        return MB85RS_ERR_TRANSFER;
    }

    SPI_Memory_CS_Deassert(fram->spi);
    return MB85RS_OK;
}

MB85RS_Error MB85RS_SetBlockProtect(MB85RS_Handle *fram, MB85RS_BlockProtect bp)
{
    if (fram == NULL || fram->spi == NULL) {
        return MB85RS_ERR_PARAM;
    }

    /* Read current status register so we preserve WPEN */
    uint8_t sr;
    MB85RS_Error err = MB85RS_ReadStatus(fram, &sr);
    if (err != MB85RS_OK) {
        return err;
    }

    /* Clear BP1:BP0, then apply the requested setting */
    sr &= ~(MB85RS_SR_BP1 | MB85RS_SR_BP0);
    sr |= (uint8_t)bp;

    return MB85RS_WriteStatus(fram, sr);
}

MB85RS_Error MB85RS_Read(MB85RS_Handle *fram,
                         uint32_t       address,
                         uint8_t       *buf,
                         uint32_t       len)
{
    MB85RS_Error err = check_bounds(fram, address, len);
    if (err != MB85RS_OK) {
        return err;
    }
    if (buf == NULL) {
        return MB85RS_ERR_PARAM;
    }

    /*
     * READ transaction layout on the bus:
     *
     *   TX: [READ][A2][A1][A0][0x00 x len]   — header + dummy bytes
     *   RX: [xx  ][xx][xx][xx][D0 .. Dn-1]   — header garbage + real data
     *
     * We allocate a single contiguous TX buffer of (header + len) bytes and
     * a matching RX buffer of the same size.  After the transfer, the data
     * lives in rxBuf starting at offset MB85RS_MEM_HDR_LEN.
     *
     * Stack allocation is fine for small reads; for large reads the caller
     * should be aware.  A future revision could use a static staging buffer.
     */
    uint32_t total = MB85RS_MEM_HDR_LEN + len;

    /* Use heap would require malloc — stay on stack for now with a guard */
    if (total > 512U) {
        /*
         * Transfers larger than 512 bytes need a different strategy.
         * For now return an error; chunked reads can be added later.
         */
        return MB85RS_ERR_PARAM;
    }

    uint8_t tx[512];
    uint8_t rx[512];

    memset(tx, 0x00, total);
    tx[0] = MB85RS_CMD_READ;
    pack_address(tx, address);

    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_Arm(fram->spi, tx, rx, (uint16_t)total, SPI_MEM_MODE_FULL_DUPLEX);

    if (!SPI_Memory_Wait(fram->spi)) {
        SPI_Memory_CS_Deassert(fram->spi);
        return MB85RS_ERR_TRANSFER;
    }

    SPI_Memory_CS_Deassert(fram->spi);

    /* Copy the payload out of the RX buffer, skipping the header bytes */
    memcpy(buf, &rx[MB85RS_MEM_HDR_LEN], len);
    return MB85RS_OK;
}

MB85RS_Error MB85RS_Write(MB85RS_Handle *fram,
                          uint32_t       address,
                          const uint8_t *buf,
                          uint32_t       len)
{
    MB85RS_Error err = check_bounds(fram, address, len);
    if (err != MB85RS_OK) {
        return err;
    }
    if (buf == NULL) {
        return MB85RS_ERR_PARAM;
    }

    /* Guard stack buffer the same way as Read */
    uint32_t total = MB85RS_MEM_HDR_LEN + len;
    if (total > 512U) {
        return MB85RS_ERR_PARAM;
    }

    /*
     * WRITE sequence requires two separate CS pulses:
     *
     *   Pulse 1:  CS↓ — WREN (0x06) — CS↑
     *   Pulse 2:  CS↓ — WRITE op-code + address + data — CS↑
     *
     * wren() provides pulse 1 as a complete, self-contained transaction.
     */
    err = wren(fram);
    if (err != MB85RS_OK) {
        return err;
    }

    /*
     * Build the TX buffer:
     *   [WRITE][A2][A1][A0][D0 .. Dn-1]
     */
    uint8_t tx[512];
    tx[0] = MB85RS_CMD_WRITE;
    pack_address(tx, address);
    memcpy(&tx[MB85RS_MEM_HDR_LEN], buf, len);

    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_Arm(fram->spi, tx, NULL, (uint16_t)total, SPI_MEM_MODE_TX_ONLY);

    if (!SPI_Memory_Wait(fram->spi)) {
        SPI_Memory_CS_Deassert(fram->spi);
        /* Attempt WRDI regardless to leave the device protected */
        (void)wrdi(fram);
        return MB85RS_ERR_TRANSFER;
    }

    SPI_Memory_CS_Deassert(fram->spi);

    /* Re-protect immediately after the write completes */
    err = wrdi(fram);
    return err;
}

MB85RS_Error MB85RS_Sleep(MB85RS_Handle *fram)
{
    if (fram == NULL || fram->spi == NULL) {
        return MB85RS_ERR_PARAM;
    }
    if (fram->sleeping) {
        return MB85RS_OK;   /* Already asleep — nothing to do */
    }

    MB85RS_Error err = send_cmd_only(fram, MB85RS_CMD_SLEEP);
    if (err == MB85RS_OK) {
        fram->sleeping = true;
    }
    return err;
}

MB85RS_Error MB85RS_Wake(MB85RS_Handle *fram)
{
    if (fram == NULL || fram->spi == NULL) {
        return MB85RS_ERR_PARAM;
    }
    if (!fram->sleeping) {
        return MB85RS_ERR_PARAM;
    }

    /*
     * Wake sequence (datasheet page 10):
     *   1. Assert CS  — the falling edge starts the tREC window inside the device.
     *   2. Deassert CS immediately — we do NOT hold CS low for the full tREC period.
     *      Holding it low risks the device interpreting later transitions as a command.
     *   3. Wait tREC (max 400 µs) before issuing any command.
     */
    SPI_Memory_CS_Assert(fram->spi);
    SPI_Memory_CS_Deassert(fram->spi);

    /* Busy-wait for tREC.  Tune MB85RS_TREC_DELAY_US to your clock. */
    volatile uint32_t delay = MB85RS_TREC_DELAY_US * 32U; /* ~1 iter per ~1µs at 32MHz */
    while (delay--) { __asm("NOP"); }

    fram->sleeping = false;
    return MB85RS_OK;
}