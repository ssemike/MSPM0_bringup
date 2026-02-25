// /*
//  * BQ27Z7x General Program Header File
//  * Initial version for BQ27Z746YAHR gauge + protector
//  *
//  * NOTE: This header focuses on core measurement, gauging and protection
//  * commands. Security / authentication commands are intentionally omitted.
//  */

// #ifndef BQ27Z7_H
// #define BQ27Z7_H

// #include <stdint.h>

// /* -------------------------------------------------------------------------- */
// /* General configuration                                                      */
// /* -------------------------------------------------------------------------- */

// /* 7-bit I2C address for BQ27Z746 (8-bit address 0xAA used in TRM examples) */
// #define BQ27Z7_DEV_ADDR      0x55

// /* Convenience defines for read / write types in helper functions */
// #define BQ27Z7_R             0u
// #define BQ27Z7_W             1u

// /* -------------------------------------------------------------------------- */
// /* Standard data commands (TRM: Data Commands, Standard Data Commands)        */
// /* Each command is a 1-byte address; multi-byte values are little-endian.     */
// /* -------------------------------------------------------------------------- */

// /* 0x00 / 0x01: ManufacturerAccess() / ControlStatus() (host command / status) */
// #define BQ27Z7_CMD_MANUFACTURER_ACCESS   0x00u

// /* 0x02 / 0x03: AtRate() (signed, mA) */
// #define BQ27Z7_CMD_AT_RATE               0x02u

// /* 0x04 / 0x05: AtRateTimeToEmpty() (minutes) */
// #define BQ27Z7_CMD_AT_RATE_TTE           0x04u

// /* 0x06 / 0x07: Temperature() (0.1 K) */
// #define BQ27Z7_CMD_TEMPERATURE           0x06u

// /* 0x08 / 0x09: Voltage() (mV) */
// #define BQ27Z7_CMD_VOLTAGE               0x08u

// /* 0x0A / 0x0B: BatteryStatus() (bit field) */
// #define BQ27Z7_CMD_BATTERY_STATUS        0x0Au

// /* 0x0C / 0x0D: Current() (signed, mA) */
// #define BQ27Z7_CMD_CURRENT               0x0Cu

// /* 0x10 / 0x11: RemainingCapacity() (mAh) */
// #define BQ27Z7_CMD_REMAINING_CAPACITY    0x10u

// /* 0x12 / 0x13: FullChargeCapacity() (mAh) */
// #define BQ27Z7_CMD_FULL_CHARGE_CAPACITY  0x12u

// /* 0x14 / 0x15: AverageCurrent() (signed, mA) */
// #define BQ27Z7_CMD_AVERAGE_CURRENT       0x14u

// /* 0x16 / 0x17: AverageTimeToEmpty() (minutes) */
// #define BQ27Z7_CMD_AVG_TIME_TO_EMPTY     0x16u

// /* 0x18 / 0x19: AverageTimeToFull() (minutes) */
// #define BQ27Z7_CMD_AVG_TIME_TO_FULL      0x18u

// /* 0x1E / 0x1F: MaxLoadCurrent() (mA) */
// #define BQ27Z7_CMD_MAX_LOAD_CURRENT      0x1Eu

// /* 0x20 / 0x21: MaxLoadTimeToEmpty() (minutes) */
// #define BQ27Z7_CMD_MAX_LOAD_TTE          0x20u

// /* 0x22 / 0x23: AveragePower() (mW) */
// #define BQ27Z7_CMD_AVERAGE_POWER         0x22u

// /* 0x28 / 0x29: InternalTemperature() (0.1 K) */
// #define BQ27Z7_CMD_INTERNAL_TEMPERATURE  0x28u

// /* 0x2A / 0x2B: CycleCount() (count) */
// #define BQ27Z7_CMD_CYCLE_COUNT           0x2Au

// /* 0x2C / 0x2D: RelativeStateOfCharge() (RSOC, %) */
// #define BQ27Z7_CMD_RSOC                  0x2Cu

// /* 0x2E / 0x2F: StateOfHealth() (SOH, %) */
// #define BQ27Z7_CMD_SOH                   0x2Eu

// /* 0x30 / 0x31: ChargingVoltage() (mV) */
// #define BQ27Z7_CMD_CHARGING_VOLTAGE      0x30u

// /* 0x32 / 0x33: ChargingCurrent() (mA) */
// #define BQ27Z7_CMD_CHARGING_CURRENT      0x32u

// /* 0x34 / 0x35: TerminateVoltage() (mV) */
// #define BQ27Z7_CMD_TERMINATE_VOLTAGE     0x34u

// /* 0x3C / 0x3D: DesignCapacity() (mAh) */
// #define BQ27Z7_CMD_DESIGN_CAPACITY       0x3Cu

// /* 0x3E / 0x3F: AltManufacturerAccess() */
// #define BQ27Z7_CMD_ALT_MANUFACTURER_ACCESS 0x3Eu

// /* 0x40–0x5F: MACData() buffer, 0x60: MACDataChecksum(), 0x61: MACDataLength() */
// #define BQ27Z7_CMD_MAC_DATA              0x40u
// #define BQ27Z7_CMD_MAC_DATA_CHECKSUM     0x60u
// #define BQ27Z7_CMD_MAC_DATA_LENGTH       0x61u

// /* 0x62–0x69 / 0x6A–0x6F: Voltage/temperature and SOC interrupt thresholds */
// #define BQ27Z7_CMD_VOLT_HI_SET           0x62u
// #define BQ27Z7_CMD_VOLT_HI_CLEAR         0x64u
// #define BQ27Z7_CMD_VOLT_LO_SET           0x66u
// #define BQ27Z7_CMD_VOLT_LO_CLEAR         0x68u
// #define BQ27Z7_CMD_TEMP_HI_SET           0x6Au
// #define BQ27Z7_CMD_TEMP_HI_CLEAR         0x6Bu
// #define BQ27Z7_CMD_TEMP_LO_SET           0x6Cu
// #define BQ27Z7_CMD_TEMP_LO_CLEAR         0x6Du
// #define BQ27Z7_CMD_INTERRUPT_STATUS      0x6Eu
// #define BQ27Z7_CMD_SOC_SET_DELTA_THRESH  0x6Fu

// /* -------------------------------------------------------------------------- */
// /* Selected ManufacturerAccess and AltManufacturerAccess command codes        */
// /* (Used via MACData() / AltManufacturerAccess(); security-related codes      */
// /* are intentionally omitted.)                                                */
// /* -------------------------------------------------------------------------- */

// /* Example status words returned via ManufacturerAccess() / AltManufacturerAccess() */
// #define BQ27Z7_MFG_ACCESS_CONTROL_STATUS   0x0000u  /* ControlStatus() */
// #define BQ27Z7_MFG_ACCESS_OPERATION_STATUS 0x0054u  /* OperationStatus() */
// #define BQ27Z7_MFG_ACCESS_CHARGING_STATUS  0x0055u  /* ChargingStatus() */

// /* -------------------------------------------------------------------------- */
// /* Helper macros                                                              */
// /* -------------------------------------------------------------------------- */

// /* Convert 0.1 K units to °C as integer (rounded) */
// static inline int16_t BQ27Z7_TenthKelvin_To_DegC(uint16_t value_0p1K)
// {
//     /* T[°C] = T[0.1K] / 10 - 273.15  → approximate as integer */
//     int32_t t_c_x10 = (int32_t)value_0p1K - 2731; /* value_0p1K/10*10 - 273.1*10 */
//     return (int16_t)(t_c_x10 / 10);
// }

// #endif /* BQ27Z7_H */

