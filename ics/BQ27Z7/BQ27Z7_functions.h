#ifndef BQ27Z746_FUNCTIONS_H
#define BQ27Z746_FUNCTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "HAL/i2c.h"
#include "gauge.h"

// ================================================================
// Device Identity
// ================================================================
#define BQ27Z746_DEVICE_TYPE    0x7746u

// ================================================================
// Standard Command Registers
// ================================================================
#define BQ27Z746_REG_MANUFACTURERACCESS     0x00u
#define BQ27Z746_REG_ATRATE                 0x02u
#define BQ27Z746_REG_TEMPERATURE            0x06u
#define BQ27Z746_REG_VOLTAGE                0x08u
#define BQ27Z746_REG_BATTERYSTATUS          0x0Au
#define BQ27Z746_REG_CURRENT                0x0Cu
#define BQ27Z746_REG_REMAININGCAPACITY      0x10u
#define BQ27Z746_REG_FULLCHARGECAPACITY     0x12u
#define BQ27Z746_REG_AVERAGECURRENT         0x14u
#define BQ27Z746_REG_AVERAGETIMETOEMPTY     0x16u
#define BQ27Z746_REG_AVERAGETIMETOFULL      0x18u
#define BQ27Z746_REG_AVERAGEPOWER           0x22u
#define BQ27Z746_REG_INTERNALTEMPERATURE    0x28u
#define BQ27Z746_REG_CYCLECOUNT             0x2Au
#define BQ27Z746_REG_RELATIVESTATEOFCHARGE  0x2Cu
#define BQ27Z746_REG_STATEOFHEALTH          0x2Eu
#define BQ27Z746_REG_ALTMANUFACTURERACCESS  0x3Eu

// ================================================================
// MAC Command Codes
// ================================================================
#define BQ27Z746_MAC_DEVICETYPE         0x0001u
#define BQ27Z746_MAC_FIRMWAREVERSION    0x0002u
#define BQ27Z746_MAC_CHEMID             0x0006u
#define BQ27Z746_MAC_RESET              0x0012u
#define BQ27Z746_MAC_OPERATIONSTATUS    0x0054u
#define BQ27Z746_MAC_CHARGINGSTATUS     0x0055u
#define BQ27Z746_MAC_GAUGINGSTATUS      0x0056u

// ================================================================
// BatteryStatus bits (0x0A)
// ================================================================
#define BQ27Z746_STATUS_FD      (1u << 4)   // Fully Discharged
#define BQ27Z746_STATUS_FC      (1u << 5)   // Fully Charged
#define BQ27Z746_STATUS_DSG     (1u << 6)   // Discharging
#define BQ27Z746_STATUS_INIT    (1u << 7)
#define BQ27Z746_STATUS_RCA     (1u << 9)   // Remaining Capacity Alarm
#define BQ27Z746_STATUS_TDA     (1u << 11)  // Terminate Discharge Alarm
#define BQ27Z746_STATUS_OTA     (1u << 12)  // Over Temperature Alarm
#define BQ27Z746_STATUS_OCA     (1u << 14)  // Over Charge Alarm

// ================================================================
// MAC frame constants
// ================================================================
#define BQ27Z746_MAC_DATA_LEN       32u
#define BQ27Z746_MAC_OVERHEAD       4u
#define BQ27Z746_MAC_FRAME_LEN      (BQ27Z746_MAC_DATA_LEN + BQ27Z746_MAC_OVERHEAD)

// ================================================================
// Telemetry cache
// ================================================================
typedef struct {
    uint16_t voltage_mV;
    int16_t  current_mA;
    int16_t  avgCurrent_mA;
    uint8_t  soc_pct;
    uint16_t remainingCap_mAh;
    uint16_t fullChargeCap_mAh;
    uint8_t  stateOfHealth_pct;
    int16_t  temperature_C;
    int16_t  internalTemp_C;
    uint16_t timeToEmpty_min;
    uint16_t timeToFull_min;
    uint16_t cycleCount;
    int16_t  avgPower_mW;
    uint16_t batteryStatus;
} BQ27Z746_Telemetry_t;

// ================================================================
// Public API
// ================================================================
bool BQ27Z746_Init(I2C_Regs *i2c);

// MAC layer
bool BQ27Z746_MAC_Read(I2C_Regs *i2c, uint16_t cmd, uint8_t *pData, uint8_t *pLen);
bool BQ27Z746_MAC_Send(I2C_Regs *i2c, uint16_t cmd);

// Diagnostic MAC reads
bool BQ27Z746_GetDeviceType(I2C_Regs *i2c, uint16_t *pType);
bool BQ27Z746_GetFirmwareVersion(I2C_Regs *i2c, uint16_t *pVersion);
bool BQ27Z746_GetChemID(I2C_Regs *i2c, uint16_t *pChemID);
bool BQ27Z746_GetOperationStatus(I2C_Regs *i2c, uint32_t *pStatus);
bool BQ27Z746_GetChargingStatus(I2C_Regs *i2c, uint16_t *pStatus);
bool BQ27Z746_GetGaugingStatus(I2C_Regs *i2c, uint32_t *pStatus);

// Live reads
uint16_t BQ27Z746_ReadVoltage_mV(I2C_Regs *i2c);
int16_t  BQ27Z746_ReadCurrent_mA(I2C_Regs *i2c);
int16_t  BQ27Z746_ReadAvgCurrent_mA(I2C_Regs *i2c);
uint8_t  BQ27Z746_ReadSOC_pct(I2C_Regs *i2c);
uint16_t BQ27Z746_ReadRemainingCap_mAh(I2C_Regs *i2c);
uint16_t BQ27Z746_ReadFullChargeCap_mAh(I2C_Regs *i2c);
uint8_t  BQ27Z746_ReadStateOfHealth_pct(I2C_Regs *i2c);
int16_t  BQ27Z746_ReadTemperature_C(I2C_Regs *i2c);
uint16_t BQ27Z746_ReadCycleCount(I2C_Regs *i2c);
uint16_t BQ27Z746_ReadBatteryStatus(I2C_Regs *i2c);
int16_t  BQ27Z746_ReadInternalTemp_C(I2C_Regs *i2c);
uint16_t BQ27Z746_ReadTimeToEmpty_min(I2C_Regs *i2c);
uint16_t BQ27Z746_ReadTimeToFull_min(I2C_Regs *i2c);
int16_t  BQ27Z746_ReadAvgPower_mW(I2C_Regs *i2c);


// Cache
void     BQ27Z746_UpdateTelemetry(I2C_Regs *i2c);
uint16_t BQ27Z746_Get_Voltage_mV(void);
int16_t  BQ27Z746_Get_Current_mA(void);
int16_t  BQ27Z746_Get_AvgCurrent_mA(void);
uint8_t  BQ27Z746_Get_SOC_pct(void);
uint16_t BQ27Z746_Get_RemainingCap_mAh(void);
uint16_t BQ27Z746_Get_FullChargeCap_mAh(void);
uint8_t  BQ27Z746_Get_StateOfHealth_pct(void);
int16_t  BQ27Z746_Get_Temperature_C(void);
uint16_t BQ27Z746_Get_CycleCount(void);
uint16_t BQ27Z746_Get_BatteryStatus(void);

// Status helpers
bool BQ27Z746_IsFullyCharged(void);
bool BQ27Z746_IsFullyDischarged(void);
bool BQ27Z746_IsDischarging(void);

// Golden Image support (highly recommended)
bool BQ27Z746_LoadGoldenImage(I2C_Regs *i2c, const char *fs_string);

uint16_t BQ27Z746_Get_TimeToEmpty_min(void);
uint16_t BQ27Z746_Get_TimeToFull_min(void);
int16_t  BQ27Z746_Get_AvgPower_mW(void);
int16_t  BQ27Z746_Get_InternalTemp_C(void);

#endif