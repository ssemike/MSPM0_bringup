#ifndef MB85RS_H
#define MB85RS_H

#include <stdint.h>
#include <stdbool.h>
#include "HAL/spi_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Device Geometry
 * --------------------------------------------------------------------------- */
#define MB85RS_SIZE_BYTES       (262144U)   /* 256K x 8 = 2Mbit              */
#define MB85RS_MAX_ADDRESS      (0x3FFFFU)  /* Last valid address             */
#define MB85RS_ADDR_BYTES       (3U)        /* 24-bit address, 3 bytes        */

/* ---------------------------------------------------------------------------
 * Op-Codes  (from datasheet Table, page 5)
 * --------------------------------------------------------------------------- */
#define MB85RS_CMD_WREN         (0x06U)     /* Set Write Enable Latch         */
#define MB85RS_CMD_WRDI         (0x04U)     /* Reset Write Enable Latch       */
#define MB85RS_CMD_RDSR         (0x05U)     /* Read Status Register           */
#define MB85RS_CMD_WRSR         (0x01U)     /* Write Status Register          */
#define MB85RS_CMD_READ         (0x03U)     /* Read Memory                    */
#define MB85RS_CMD_WRITE        (0x02U)     /* Write Memory                   */
#define MB85RS_CMD_RDID         (0x9FU)     /* Read Device ID                 */
#define MB85RS_CMD_FSTRD        (0x0BU)     /* Fast Read Memory               */
#define MB85RS_CMD_SLEEP        (0xB9U)     /* Enter Sleep Mode               */

/* ---------------------------------------------------------------------------
 * Status Register Bits  (page 5)
 * --------------------------------------------------------------------------- */
#define MB85RS_SR_WPEN          (1U << 7)   /* Status Register Write Protect  */
#define MB85RS_SR_BP1           (1U << 3)   /* Block Protect bit 1            */
#define MB85RS_SR_BP0           (1U << 2)   /* Block Protect bit 0            */
#define MB85RS_SR_WEL           (1U << 1)   /* Write Enable Latch (read-only) */

/* ---------------------------------------------------------------------------
 * Device ID  (page 9)
 * --------------------------------------------------------------------------- */
#define MB85RS_ID_MANUFACTURER  (0x04U)     /* Fujitsu / RAMXEED              */
#define MB85RS_ID_CONTINUATION  (0x7FU)     /* Continuation code              */
#define MB85RS_ID_PRODUCT1      (0x48U)     /* Product ID byte 1 (2Mbit)      */
#define MB85RS_ID_PRODUCT2      (0x03U)     /* Product ID byte 2              */
#define MB85RS_ID_TOTAL_BYTES   (4U)        /* Total bytes returned by RDID   */

/* ---------------------------------------------------------------------------
 * Command Buffer Sizes
 *
 * These define the total number of bytes that must be clocked on the bus
 * for each command type.  Used to size local buffers and DMA transfers.
 *
 *  WREN / WRDI / SLEEP : 1 byte  (op-code only)
 *  RDSR                : 1 (op-code) + 1 (data out)     = 2
 *  WRSR                : 1 (op-code) + 1 (data in)      = 2
 *  RDID                : 1 (op-code) + 4 (id bytes out) = 5
 *  READ / WRITE        : 1 (op-code) + 3 (address)      = 4  header
 *                        + N data bytes                  (variable)
 *  FSTRD               : 1 (op-code) + 3 (address)
 *                        + 1 (dummy)                     = 5  header
 *                        + N data bytes                  (variable)
 * --------------------------------------------------------------------------- */
#define MB85RS_CMD_ONLY_LEN     (1U)
#define MB85RS_RDSR_LEN         (2U)
#define MB85RS_WRSR_LEN         (2U)
#define MB85RS_RDID_LEN         (5U)        /* op-code + 4 id bytes           */
#define MB85RS_MEM_HDR_LEN      (4U)        /* op-code + 3 address bytes      */
#define MB85RS_FSTRD_HDR_LEN    (5U)        /* op-code + 3 address + 1 dummy  */

/* ---------------------------------------------------------------------------
 * Sleep Recovery
 * tREC max = 400 µs  (datasheet AC characteristics, page 14)
 * The delay loop in MB85RS_Wake uses this count — tune to your CPU clock.
 * --------------------------------------------------------------------------- */
#define MB85RS_TREC_DELAY_US    (400U)

/* ---------------------------------------------------------------------------
 * Error Codes
 * --------------------------------------------------------------------------- */
typedef enum {
    MB85RS_OK               =  0,   /* Success                                */
    MB85RS_ERR_PARAM        = -1,   /* Bad argument (NULL pointer, OOB addr)  */
    MB85RS_ERR_TRANSFER     = -2,   /* SPI transfer timed out or failed       */
    MB85RS_ERR_DEVICE_ID    = -3,   /* RDID response does not match           */
    MB85RS_ERR_BOUNDS       = -4,   /* offset + len would exceed device size  */
} MB85RS_Error;

/* ---------------------------------------------------------------------------
 * Block Protect Settings
 * Mirrors the BP1:BP0 truth table from the datasheet (page 11)
 * --------------------------------------------------------------------------- */
typedef enum {
    MB85RS_BP_NONE          = 0x00, /* No protection                          */
    MB85RS_BP_UPPER_QUARTER = 0x04, /* 0x30000 – 0x3FFFF protected            */
    MB85RS_BP_UPPER_HALF    = 0x08, /* 0x20000 – 0x3FFFF protected            */
    MB85RS_BP_ALL           = 0x0C, /* Entire array protected                 */
} MB85RS_BlockProtect;

/* ---------------------------------------------------------------------------
 * Device Handle
 *
 * One instance per physical MB85RS2MTA on the board.
 * The caller owns the SPI_Memory_Handle and passes a pointer here so the
 * FRAM driver never needs to know about DMA channels or GPIO registers
 * directly — it just calls the spi_mem API.
 * --------------------------------------------------------------------------- */
typedef struct {
    SPI_Memory_Handle  *spi;        /* Pointer to the initialised SPI handle  */
    bool                sleeping;   /* true while device is in SLEEP mode     */
    bool                initialised;/* true after MB85RS_Init succeeds        */
} MB85RS_Handle;

/* ---------------------------------------------------------------------------
 * API
 * --------------------------------------------------------------------------- */

/**
 * @brief  Initialise the FRAM driver and verify the device is present.
 *
 *         Calls RDID and validates the four-byte device ID against the
 *         expected values for the MB85RS2MTA.  Must be called once before
 *         any read or write operation.
 *
 * @param  fram   Pointer to an MB85RS_Handle to initialise.
 * @param  spi    Pointer to an already-initialised SPI_Memory_Handle.
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_Init(MB85RS_Handle *fram, SPI_Memory_Handle *spi);

/**
 * @brief  Read bytes from FRAM memory.
 *
 * @param  fram     Pointer to an initialised MB85RS_Handle.
 * @param  address  Starting address (0x00000 – 0x3FFFF).
 * @param  buf      Destination buffer.  Must be at least len bytes.
 * @param  len      Number of bytes to read.
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_Read(MB85RS_Handle *fram,
                         uint32_t       address,
                         uint8_t       *buf,
                         uint32_t       len);

/**
 * @brief  Write bytes to FRAM memory.
 *
 *         Automatically issues WREN before the write and WRDI after,
 *         each in their own CS pulse as required by the datasheet.
 *
 * @param  fram     Pointer to an initialised MB85RS_Handle.
 * @param  address  Starting address (0x00000 – 0x3FFFF).
 * @param  buf      Source buffer.
 * @param  len      Number of bytes to write.
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_Write(MB85RS_Handle *fram,
                          uint32_t       address,
                          const uint8_t *buf,
                          uint32_t       len);

/**
 * @brief  Read the 8-bit status register.
 *
 * @param  fram   Pointer to an initialised MB85RS_Handle.
 * @param  sr     Output: value of the status register.
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_ReadStatus(MB85RS_Handle *fram, uint8_t *sr);

/**
 * @brief  Write the 8-bit status register.
 *
 *         Only WPEN, BP1, BP0 are writable.  WEL (bit 1) and bit 0 are
 *         ignored by the device even if set in value.
 *         Automatically issues WREN before the write.
 *
 * @param  fram   Pointer to an initialised MB85RS_Handle.
 * @param  value  Byte to write to the status register.
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_WriteStatus(MB85RS_Handle *fram, uint8_t value);

/**
 * @brief  Configure the block-protect level.
 *
 *         Convenience wrapper around MB85RS_WriteStatus that sets only
 *         the BP1:BP0 field, preserving the WPEN bit.
 *
 * @param  fram   Pointer to an initialised MB85RS_Handle.
 * @param  bp     Desired protection level (MB85RS_BlockProtect enum).
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_SetBlockProtect(MB85RS_Handle *fram, MB85RS_BlockProtect bp);

/**
 * @brief  Read and validate the four-byte device ID.
 *
 *         Returns MB85RS_ERR_DEVICE_ID if the response does not match the
 *         expected Manufacturer ID, Continuation code, and Product IDs.
 *
 * @param  fram   Pointer to an initialised MB85RS_Handle.
 *
 * @return MB85RS_OK on success, MB85RS_ERR_DEVICE_ID on mismatch.
 */
MB85RS_Error MB85RS_ReadID(MB85RS_Handle *fram);

/**
 * @brief  Put the device into SLEEP mode.
 *
 *         After this call the device ignores SCK and SI.  Use MB85RS_Wake
 *         to return to normal operation.
 *
 * @param  fram   Pointer to an initialised MB85RS_Handle.
 *
 * @return MB85RS_OK on success, negative MB85RS_Error on failure.
 */
MB85RS_Error MB85RS_Sleep(MB85RS_Handle *fram);

/**
 * @brief  Wake the device from SLEEP mode.
 *
 *         Asserts CS to begin the tREC recovery window, waits MB85RS_TREC_DELAY_US,
 *         then deasserts CS.  The device is ready for commands after this returns.
 *
 * @param  fram   Pointer to an initialised MB85RS_Handle.
 *
 * @return MB85RS_OK on success, MB85RS_ERR_PARAM if not sleeping.
 */
MB85RS_Error MB85RS_Wake(MB85RS_Handle *fram);

#ifdef __cplusplus
}
#endif

#endif /* MB85RS_H */