// /*
//  * BQ27Z7x General Program Functions File
//  * Initial version for BQ27Z746YAHR gauge + protector
//  *
//  * This module uses the shared I2C controller to communicate with the
//  * BQ27Z7x device and exposes a small, convenient API for firmware to
//  * access the most important measurement and gauging results.
//  *
//  * Security / authentication flows are intentionally not covered here.
//  */

// #include <stdbool.h>
// #include <stdio.h>
// #include <sys/_stdint.h>

// #include "ti_msp_dl_config.h"

// #include "BQ27Z7.h"
// #include "BQ27Z7_functions.h"
// #include "BQ7690x_i2c.h"

// /* -------------------------------------------------------------------------- */
// /* Internal cached data                                                       */
// /* -------------------------------------------------------------------------- */

// // Measurements
// static int16_t  g_bq27z7_voltage_mV            = 0;
// static int16_t  g_bq27z7_current_mA            = 0;
// static int16_t  g_bq27z7_avgCurrent_mA         = 0;
// static int16_t  g_bq27z7_temp_C                = 0;
// static int16_t  g_bq27z7_intTemp_C             = 0;
// static int16_t  g_bq27z7_avgPower_mW           = 0;

// // Gauging
// static uint16_t g_bq27z7_remCapacity_mAh       = 0;
// static uint16_t g_bq27z7_fullChargeCapacity_mAh= 0;
// static uint16_t g_bq27z7_designCapacity_mAh    = 0;
// static uint16_t g_bq27z7_rsoc_percent          = 0;
// static uint16_t g_bq27z7_soh_percent           = 0;
// static uint16_t g_bq27z7_cycleCount            = 0;
// static uint16_t g_bq27z7_avgTimeToEmpty_min    = 0;
// static uint16_t g_bq27z7_avgTimeToFull_min     = 0;
// static uint16_t g_bq27z7_maxLoadCurrent_mA     = 0;
// static uint16_t g_bq27z7_maxLoadTimeToEmpty_min= 0;

// // Status
// static uint16_t g_bq27z7_batteryStatus         = 0;
// static uint16_t g_bq27z7_controlStatus         = 0;
// static uint16_t g_bq27z7_operationStatus       = 0;
// static uint16_t g_bq27z7_chargingStatus        = 0;

// /* -------------------------------------------------------------------------- */
// /* Local helpers                                                              */
// /* -------------------------------------------------------------------------- */

// // Read a 2-byte little-endian word from a standard command
// static uint16_t BQ27Z7_ReadWord(uint8_t command)
// {
//     uint8_t buf[2] = {0, 0};
//     I2C_ReadDevice(BQ27Z7_DEV_ADDR, command, buf, 2);
//     return ((uint16_t)buf[1] << 8) | buf[0];
// }

// // Write a 2-byte little-endian word to a standard command
// static void BQ27Z7_WriteWord(uint8_t command, uint16_t value)
// {
//     uint8_t buf[2];
//     buf[0] = (uint8_t)(value & 0xFFu);
//     buf[1] = (uint8_t)((value >> 8) & 0xFFu);
//     I2C_WriteDevice(BQ27Z7_DEV_ADDR, command, buf, 2);
// }

// // Read a 2-byte word returned by ManufacturerAccess() / AltManufacturerAccess()
// static uint16_t BQ27Z7_ReadMfgAccessWord(uint16_t subcommand)
// {
//     uint8_t reg[2];
//     uint8_t data[2] = {0, 0};

//     // Write subcommand (little-endian) to ManufacturerAccess()
//     reg[0] = (uint8_t)(subcommand & 0xFFu);
//     reg[1] = (uint8_t)((subcommand >> 8) & 0xFFu);

//     // Subcommand write to 0x00 / 0x01
//     I2C_WriteDevice(BQ27Z7_DEV_ADDR, BQ27Z7_CMD_MANUFACTURER_ACCESS, reg, 2);

//     // Small delay to allow device to process command
//     delay_cycles(1000);

//     // Read back 2-byte response
//     I2C_ReadDevice(BQ27Z7_DEV_ADDR, BQ27Z7_CMD_MANUFACTURER_ACCESS, data, 2);

//     return ((uint16_t)data[1] << 8) | data[0];
// }

// /* -------------------------------------------------------------------------- */
// /* Public API                                                                 */
// /* -------------------------------------------------------------------------- */

// void BQ27Z7_ReadBasicMeasurements(void)
// {
//     uint16_t t_raw;

//     // Pack voltage (mV)
//     g_bq27z7_voltage_mV = (int16_t)BQ27Z7_ReadWord(BQ27Z7_CMD_VOLTAGE);

//     // Current and average current (mA, signed)
//     g_bq27z7_current_mA     = (int16_t)BQ27Z7_ReadWord(BQ27Z7_CMD_CURRENT);
//     g_bq27z7_avgCurrent_mA  = (int16_t)BQ27Z7_ReadWord(BQ27Z7_CMD_AVERAGE_CURRENT);

//     // Temperature (0.1 K → °C)
//     t_raw           = BQ27Z7_ReadWord(BQ27Z7_CMD_TEMPERATURE);
//     g_bq27z7_temp_C = BQ27Z7_TenthKelvin_To_DegC(t_raw);

//     // Internal die temperature
//     t_raw               = BQ27Z7_ReadWord(BQ27Z7_CMD_INTERNAL_TEMPERATURE);
//     g_bq27z7_intTemp_C  = BQ27Z7_TenthKelvin_To_DegC(t_raw);

//     // Average power (mW)
//     g_bq27z7_avgPower_mW = (int16_t)BQ27Z7_ReadWord(BQ27Z7_CMD_AVERAGE_POWER);
// }

// void BQ27Z7_ReadGauging(void)
// {
//     g_bq27z7_remCapacity_mAh        = BQ27Z7_ReadWord(BQ27Z7_CMD_REMAINING_CAPACITY);
//     g_bq27z7_fullChargeCapacity_mAh = BQ27Z7_ReadWord(BQ27Z7_CMD_FULL_CHARGE_CAPACITY);
//     g_bq27z7_designCapacity_mAh     = BQ27Z7_ReadWord(BQ27Z7_CMD_DESIGN_CAPACITY);
//     g_bq27z7_rsoc_percent           = BQ27Z7_ReadWord(BQ27Z7_CMD_RSOC);
//     g_bq27z7_soh_percent            = BQ27Z7_ReadWord(BQ27Z7_CMD_SOH);
//     g_bq27z7_cycleCount             = BQ27Z7_ReadWord(BQ27Z7_CMD_CYCLE_COUNT);
//     g_bq27z7_avgTimeToEmpty_min     = BQ27Z7_ReadWord(BQ27Z7_CMD_AVG_TIME_TO_EMPTY);
//     g_bq27z7_avgTimeToFull_min      = BQ27Z7_ReadWord(BQ27Z7_CMD_AVG_TIME_TO_FULL);
//     g_bq27z7_maxLoadCurrent_mA      = BQ27Z7_ReadWord(BQ27Z7_CMD_MAX_LOAD_CURRENT);
//     g_bq27z7_maxLoadTimeToEmpty_min = BQ27Z7_ReadWord(BQ27Z7_CMD_MAX_LOAD_TTE);
// }

// void BQ27Z7_ReadStatus(void)
// {
//     // BatteryStatus() from standard command
//     g_bq27z7_batteryStatus   = BQ27Z7_ReadWord(BQ27Z7_CMD_BATTERY_STATUS);

//     // ControlStatus(), OperationStatus(), ChargingStatus() via ManufacturerAccess()
//     g_bq27z7_controlStatus   = BQ27Z7_ReadMfgAccessWord(BQ27Z7_MFG_ACCESS_CONTROL_STATUS);
//     g_bq27z7_operationStatus = BQ27Z7_ReadMfgAccessWord(BQ27Z7_MFG_ACCESS_OPERATION_STATUS);
//     g_bq27z7_chargingStatus  = BQ27Z7_ReadMfgAccessWord(BQ27Z7_MFG_ACCESS_CHARGING_STATUS);
// }

// /* -------------------------------------------------------------------------- */
// /* Getter implementations                                                     */
// /* -------------------------------------------------------------------------- */

// int16_t BQ27Z7_Get_Voltage_mV(void)
// {
//     return g_bq27z7_voltage_mV;
// }

// int16_t BQ27Z7_Get_Current_mA(void)
// {
//     return g_bq27z7_current_mA;
// }

// int16_t BQ27Z7_Get_AvgCurrent_mA(void)
// {
//     return g_bq27z7_avgCurrent_mA;
// }

// int16_t BQ27Z7_Get_Temperature_C(void)
// {
//     return g_bq27z7_temp_C;
// }

// int16_t BQ27Z7_Get_InternalTemperature_C(void)
// {
//     return g_bq27z7_intTemp_C;
// }

// int16_t BQ27Z7_Get_AvgPower_mW(void)
// {
//     return g_bq27z7_avgPower_mW;
// }

// uint16_t BQ27Z7_Get_RemainingCapacity_mAh(void)
// {
//     return g_bq27z7_remCapacity_mAh;
// }

// uint16_t BQ27Z7_Get_FullChargeCapacity_mAh(void)
// {
//     return g_bq27z7_fullChargeCapacity_mAh;
// }

// uint16_t BQ27Z7_Get_DesignCapacity_mAh(void)
// {
//     return g_bq27z7_designCapacity_mAh;
// }

// uint16_t BQ27Z7_Get_RSOC_percent(void)
// {
//     return g_bq27z7_rsoc_percent;
// }

// uint16_t BQ27Z7_Get_SOH_percent(void)
// {
//     return g_bq27z7_soh_percent;
// }

// uint16_t BQ27Z7_Get_CycleCount(void)
// {
//     return g_bq27z7_cycleCount;
// }

// uint16_t BQ27Z7_Get_AvgTimeToEmpty_min(void)
// {
//     return g_bq27z7_avgTimeToEmpty_min;
// }

// uint16_t BQ27Z7_Get_AvgTimeToFull_min(void)
// {
//     return g_bq27z7_avgTimeToFull_min;
// }

// uint16_t BQ27Z7_Get_MaxLoadCurrent_mA(void)
// {
//     return g_bq27z7_maxLoadCurrent_mA;
// }

// uint16_t BQ27Z7_Get_MaxLoadTimeToEmpty_min(void)
// {
//     return g_bq27z7_maxLoadTimeToEmpty_min;
// }

// uint16_t BQ27Z7_Get_BatteryStatus(void)
// {
//     return g_bq27z7_batteryStatus;
// }

// uint16_t BQ27Z7_Get_ControlStatus(void)
// {
//     return g_bq27z7_controlStatus;
// }

// uint16_t BQ27Z7_Get_OperationStatus(void)
// {
//     return g_bq27z7_operationStatus;
// }

// uint16_t BQ27Z7_Get_ChargingStatus(void)
// {
//     return g_bq27z7_chargingStatus;
// }

