// /*
//  * BQ27Z7x General Program Functions Header File
//  * Initial version for BQ27Z746YAHR gauge + protector
//  *
//  * This module provides high-level helper functions to access the most
//  * commonly used measurement, gauging and protection-related data from
//  * the BQ27Z7x family via I2C. Security / authentication flows are not
//  * covered here.
//  */

// #ifndef BQ27Z7_FUNCTIONS_H
// #define BQ27Z7_FUNCTIONS_H

// #include <stdint.h>

// /********* Core read helpers *********/

// // Read and cache a basic snapshot of pack voltage, current and temperature
// void BQ27Z7_ReadBasicMeasurements(void);

// // Read and cache core gauging results (capacity, SOC, SOH, times)
// void BQ27Z7_ReadGauging(void);

// // Read and cache key status words (BatteryStatus, OperationStatus, ChargingStatus)
// void BQ27Z7_ReadStatus(void);

// /********* Getter functions (return last cached values) *********/

// // Measurements (instantaneous)
// int16_t  BQ27Z7_Get_Voltage_mV(void);
// int16_t  BQ27Z7_Get_Current_mA(void);
// int16_t  BQ27Z7_Get_AvgCurrent_mA(void);
// int16_t  BQ27Z7_Get_Temperature_C(void);
// int16_t  BQ27Z7_Get_InternalTemperature_C(void);
// int16_t  BQ27Z7_Get_AvgPower_mW(void);

// // Gauging / capacity
// uint16_t BQ27Z7_Get_RemainingCapacity_mAh(void);
// uint16_t BQ27Z7_Get_FullChargeCapacity_mAh(void);
// uint16_t BQ27Z7_Get_DesignCapacity_mAh(void);
// uint16_t BQ27Z7_Get_RSOC_percent(void);
// uint16_t BQ27Z7_Get_SOH_percent(void);
// uint16_t BQ27Z7_Get_CycleCount(void);
// uint16_t BQ27Z7_Get_AvgTimeToEmpty_min(void);
// uint16_t BQ27Z7_Get_AvgTimeToFull_min(void);
// uint16_t BQ27Z7_Get_MaxLoadCurrent_mA(void);
// uint16_t BQ27Z7_Get_MaxLoadTimeToEmpty_min(void);

// // Status words
// uint16_t BQ27Z7_Get_BatteryStatus(void);
// uint16_t BQ27Z7_Get_ControlStatus(void);
// uint16_t BQ27Z7_Get_OperationStatus(void);
// uint16_t BQ27Z7_Get_ChargingStatus(void);

// #endif /* BQ27Z7_FUNCTIONS_H */

