/*
 * BQ27Z746 High-Level Driver
 * Sits on top of gauge.c / gauge.h (mid-level) and HAL/i2c.h (hardware)
 *
 * Layer structure:
 *   BQ27Z746_functions.c  ← this file
 *       ↓
 *   gauge.c / gauge.h     (gauge_cmd_read, gauge_write, gauge_read, etc.)
 *       ↓
 *   HAL/i2c.c / i2c.h     (I2C_ReadDevice, I2C_WriteDevice)
 */

#include "BQ27Z7_functions.h"
#include <string.h>

// ================================================================
// Internal telemetry cache
// ================================================================
static BQ27Z746_Telemetry_t g_telem = {0};

// ================================================================
// Internal helper: raw 16-bit register read
// Returns the raw uint16_t from the gauge standard command register.
// ================================================================
static uint16_t read_reg16(I2C_Regs *i2c, uint8_t reg)
{
    return (uint16_t)gauge_cmd_read(i2c, reg);
}

// ================================================================
// Internal helper: Kelvin (0.1 K units) to Celsius
// BQ27Z746 reports temperature as tenths of Kelvin (e.g. 2981 = 25.1 C)
// ================================================================
static int16_t kelvin_to_celsius(uint16_t raw_01K)
{
    return (int16_t)((int32_t)raw_01K / 10 - 273);
}

// ================================================================
// MAC Layer
// ================================================================

/*
 * BQ27Z746_MAC_Read
 *
 * Protocol (from Zephyr driver / BQ27Z746 TRM):
 * 1. Write the 16-bit MAC command to ALTMANUFACTURERACCESS (0x3E)
 * 2. Read 36 bytes starting from ALTMANUFACTURERACCESS:
 *      [0..1]  = echoed command word (little-endian) — used for verification
 *      [2..33] = payload data (up to 32 bytes)
 *      [34]    = checksum: 0xFF - (sum of bytes [0..33])
 *      [35]    = total length including cmd + data + overhead
 * 3. Verify echoed command matches what was sent
 * 4. Verify checksum
 * 5. Return payload bytes [2..2+datalen] to caller
 */
bool BQ27Z746_MAC_Read(I2C_Regs *i2c, uint16_t cmd, uint8_t *pData, uint8_t *pLen)
{
    uint8_t frame[BQ27Z746_MAC_FRAME_LEN];

    /* Step 1: write command to ALTMANUFACTURERACCESS */
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = (uint8_t)(cmd & 0xFFu);
    cmd_bytes[1] = (uint8_t)((cmd >> 8u) & 0xFFu);

    if (gauge_write(i2c, BQ27Z746_REG_ALTMANUFACTURERACCESS, cmd_bytes, 2) != 2)
        return false;

    /* Step 2: read back the full 36-byte frame */
    if (gauge_read(i2c, BQ27Z746_REG_ALTMANUFACTURERACCESS, frame, BQ27Z746_MAC_FRAME_LEN)
            != BQ27Z746_MAC_FRAME_LEN)
        return false;

    /* Step 3: verify echoed command (little-endian in bytes [0..1]) */
    uint16_t echoed_cmd = (uint16_t)(frame[0] | ((uint16_t)frame[1] << 8u));
    if (echoed_cmd != cmd)
        return false;

    /* Step 4: verify checksum
     * checksum = 0xFF - (sum of bytes [0..33])
     * byte [34] holds the checksum written by the gauge */
    uint8_t sum = 0u;
    for (uint8_t i = 0u; i < (BQ27Z746_MAC_FRAME_LEN - 2u); i++)
        sum += frame[i];

    uint8_t expected_checksum = (uint8_t)(0xFFu - sum);
    if (expected_checksum != frame[34])
        return false;

    /* Step 5: extract payload
     * byte [35] = total length (cmd bytes + data + overhead)
     * actual data length = frame[35] - BQ27Z746_MAC_OVERHEAD */
    uint8_t data_len = frame[35] - (uint8_t)BQ27Z746_MAC_OVERHEAD;
    if (data_len > BQ27Z746_MAC_DATA_LEN)
        data_len = BQ27Z746_MAC_DATA_LEN;

    memcpy(pData, &frame[2], data_len);
    if (pLen != NULL)
        *pLen = data_len;

    return true;
}

/*
 * BQ27Z746_MAC_Send
 * For commands that trigger an action and return no data (reset, seal, etc.)
 */
bool BQ27Z746_MAC_Send(I2C_Regs *i2c, uint16_t cmd)
{
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = (uint8_t)(cmd & 0xFFu);
    cmd_bytes[1] = (uint8_t)((cmd >> 8u) & 0xFFu);
    return (gauge_write(i2c, BQ27Z746_REG_ALTMANUFACTURERACCESS, cmd_bytes, 2) == 2);
}

// ================================================================
// Diagnostic reads (MAC-based)
// ================================================================

bool BQ27Z746_GetDeviceType(I2C_Regs *i2c, uint16_t *pType)
{
    uint8_t data[BQ27Z746_MAC_DATA_LEN];
    uint8_t len = 0u;
    if (!BQ27Z746_MAC_Read(i2c, BQ27Z746_MAC_DEVICETYPE, data, &len))
        return false;
    if (len < 2u) return false;
    *pType = (uint16_t)(data[0] | ((uint16_t)data[1] << 8u));
    return true;
}

bool BQ27Z746_GetFirmwareVersion(I2C_Regs *i2c, uint16_t *pVersion)
{
    uint8_t data[BQ27Z746_MAC_DATA_LEN];
    uint8_t len = 0u;
    if (!BQ27Z746_MAC_Read(i2c, BQ27Z746_MAC_FIRMWAREVERSION, data, &len))
        return false;
    if (len < 2u) return false;
    *pVersion = (uint16_t)(data[0] | ((uint16_t)data[1] << 8u));
    return true;
}

bool BQ27Z746_GetChemID(I2C_Regs *i2c, uint16_t *pChemID)
{
    uint8_t data[BQ27Z746_MAC_DATA_LEN];
    uint8_t len = 0u;
    if (!BQ27Z746_MAC_Read(i2c, BQ27Z746_MAC_CHEMID, data, &len))
        return false;
    if (len < 2u) return false;
    *pChemID = (uint16_t)(data[0] | ((uint16_t)data[1] << 8u));
    return true;
}

bool BQ27Z746_GetOperationStatus(I2C_Regs *i2c, uint32_t *pStatus)
{
    uint8_t data[BQ27Z746_MAC_DATA_LEN];
    uint8_t len = 0u;
    if (!BQ27Z746_MAC_Read(i2c, BQ27Z746_MAC_OPERATIONSTATUS, data, &len))
        return false;
    if (len < 4u) return false;
    *pStatus = (uint32_t)(data[0])
             | ((uint32_t)data[1] << 8u)
             | ((uint32_t)data[2] << 16u)
             | ((uint32_t)data[3] << 24u);
    return true;
}

bool BQ27Z746_GetChargingStatus(I2C_Regs *i2c, uint16_t *pStatus)
{
    uint8_t data[BQ27Z746_MAC_DATA_LEN];
    uint8_t len = 0u;
    if (!BQ27Z746_MAC_Read(i2c, BQ27Z746_MAC_CHARGINGSTATUS, data, &len))
        return false;
    if (len < 2u) return false;
    *pStatus = (uint16_t)(data[0] | ((uint16_t)data[1] << 8u));
    return true;
}

bool BQ27Z746_GetGaugingStatus(I2C_Regs *i2c, uint32_t *pStatus)
{
    uint8_t data[BQ27Z746_MAC_DATA_LEN];
    uint8_t len = 0u;
    if (!BQ27Z746_MAC_Read(i2c, BQ27Z746_MAC_GAUGINGSTATUS, data, &len))
        return false;
    if (len < 4u) return false;
    *pStatus = (uint32_t)(data[0])
             | ((uint32_t)data[1] << 8u)
             | ((uint32_t)data[2] << 16u)
             | ((uint32_t)data[3] << 24u);
    return true;
}

// ================================================================
// Init
// ================================================================

bool BQ27Z746_Init(I2C_Regs *i2c)
{
    gauge_address(i2c, GAUGE_I2C_ADDR);

    uint16_t device_type = 0u;
    if (!BQ27Z746_GetDeviceType(i2c, &device_type))
        return false;

    return true;
}

// ================================================================
// Live register reads
// ================================================================

uint16_t BQ27Z746_ReadVoltage_mV(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_VOLTAGE);
}

int16_t BQ27Z746_ReadCurrent_mA(I2C_Regs *i2c)
{
    return (int16_t)read_reg16(i2c, BQ27Z746_REG_CURRENT);
}

int16_t BQ27Z746_ReadAvgCurrent_mA(I2C_Regs *i2c)
{
    return (int16_t)read_reg16(i2c, BQ27Z746_REG_AVERAGECURRENT);
}

uint8_t BQ27Z746_ReadSOC_pct(I2C_Regs *i2c)
{
    return (uint8_t)read_reg16(i2c, BQ27Z746_REG_RELATIVESTATEOFCHARGE);
}

uint16_t BQ27Z746_ReadRemainingCap_mAh(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_REMAININGCAPACITY);
}

uint16_t BQ27Z746_ReadFullChargeCap_mAh(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_FULLCHARGECAPACITY);
}

uint8_t BQ27Z746_ReadStateOfHealth_pct(I2C_Regs *i2c)
{
    return (uint8_t)read_reg16(i2c, BQ27Z746_REG_STATEOFHEALTH);
}

int16_t BQ27Z746_ReadTemperature_C(I2C_Regs *i2c)
{
    return kelvin_to_celsius(read_reg16(i2c, BQ27Z746_REG_TEMPERATURE));
}

int16_t BQ27Z746_ReadInternalTemp_C(I2C_Regs *i2c)
{
    return kelvin_to_celsius(read_reg16(i2c, BQ27Z746_REG_INTERNALTEMPERATURE));
}

uint16_t BQ27Z746_ReadTimeToEmpty_min(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_AVERAGETIMETOEMPTY);
}

uint16_t BQ27Z746_ReadTimeToFull_min(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_AVERAGETIMETOFULL);
}

uint16_t BQ27Z746_ReadCycleCount(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_CYCLECOUNT);
}

int16_t BQ27Z746_ReadAvgPower_mW(I2C_Regs *i2c)
{
    return (int16_t)read_reg16(i2c, BQ27Z746_REG_AVERAGEPOWER);
}

uint16_t BQ27Z746_ReadBatteryStatus(I2C_Regs *i2c)
{
    return read_reg16(i2c, BQ27Z746_REG_BATTERYSTATUS);
}

// ================================================================
// Cache: UpdateTelemetry + getters
// ================================================================

void BQ27Z746_UpdateTelemetry(I2C_Regs *i2c)
{
    g_telem.voltage_mV         = BQ27Z746_ReadVoltage_mV(i2c);
    g_telem.current_mA         = BQ27Z746_ReadCurrent_mA(i2c);
    g_telem.avgCurrent_mA      = BQ27Z746_ReadAvgCurrent_mA(i2c);
    g_telem.soc_pct            = BQ27Z746_ReadSOC_pct(i2c);
    g_telem.remainingCap_mAh   = BQ27Z746_ReadRemainingCap_mAh(i2c);
    g_telem.fullChargeCap_mAh  = BQ27Z746_ReadFullChargeCap_mAh(i2c);
    g_telem.stateOfHealth_pct  = BQ27Z746_ReadStateOfHealth_pct(i2c);
    g_telem.temperature_C      = BQ27Z746_ReadTemperature_C(i2c);
    g_telem.internalTemp_C     = BQ27Z746_ReadInternalTemp_C(i2c);
    g_telem.timeToEmpty_min    = BQ27Z746_ReadTimeToEmpty_min(i2c);
    g_telem.timeToFull_min     = BQ27Z746_ReadTimeToFull_min(i2c);
    g_telem.cycleCount         = BQ27Z746_ReadCycleCount(i2c);
    g_telem.avgPower_mW        = BQ27Z746_ReadAvgPower_mW(i2c);
    g_telem.batteryStatus      = BQ27Z746_ReadBatteryStatus(i2c);
}

uint16_t BQ27Z746_Get_Voltage_mV(void)        { return g_telem.voltage_mV; }
int16_t  BQ27Z746_Get_Current_mA(void)        { return g_telem.current_mA; }
int16_t  BQ27Z746_Get_AvgCurrent_mA(void)     { return g_telem.avgCurrent_mA; }
uint8_t  BQ27Z746_Get_SOC_pct(void)           { return g_telem.soc_pct; }
uint16_t BQ27Z746_Get_RemainingCap_mAh(void)  { return g_telem.remainingCap_mAh; }
uint16_t BQ27Z746_Get_FullChargeCap_mAh(void) { return g_telem.fullChargeCap_mAh; }
uint8_t  BQ27Z746_Get_StateOfHealth_pct(void) { return g_telem.stateOfHealth_pct; }
int16_t  BQ27Z746_Get_Temperature_C(void)     { return g_telem.temperature_C; }
int16_t  BQ27Z746_Get_InternalTemp_C(void)    { return g_telem.internalTemp_C; }
uint16_t BQ27Z746_Get_TimeToEmpty_min(void)   { return g_telem.timeToEmpty_min; }
uint16_t BQ27Z746_Get_TimeToFull_min(void)    { return g_telem.timeToFull_min; }
uint16_t BQ27Z746_Get_CycleCount(void)        { return g_telem.cycleCount; }
int16_t  BQ27Z746_Get_AvgPower_mW(void)       { return g_telem.avgPower_mW; }
uint16_t BQ27Z746_Get_BatteryStatus(void)     { return g_telem.batteryStatus; }

// ================================================================
// Status bit helpers (operate on cached BatteryStatus)
// ================================================================

bool BQ27Z746_IsFullyCharged(void)
{
    return (g_telem.batteryStatus & BQ27Z746_STATUS_FC) != 0u;
}

bool BQ27Z746_IsFullyDischarged(void)
{
    return (g_telem.batteryStatus & BQ27Z746_STATUS_FD) != 0u;
}

bool BQ27Z746_IsDischarging(void)
{
    return (g_telem.batteryStatus & BQ27Z746_STATUS_DSG) != 0u;
}

// ================================================================
// Golden Image wrapper (one-liner usage)
// ================================================================
bool BQ27Z746_LoadGoldenImage(I2C_Regs *i2c, const char *fs_string)
{
    char *result = gauge_execute_fs(i2c, (char *)fs_string);
    return (result == NULL || *result == '\0');
}