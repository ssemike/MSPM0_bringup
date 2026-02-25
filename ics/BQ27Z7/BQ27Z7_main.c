// /**
//  ******************************************************************************
//  * Filename : BQ27Z7_main.c
//  * Main program body for BQ27Z746YAHR fuel gauge + protector
//  * Initial version
//  *
//  * This example mirrors the structure of BQ7690x_main.c but talks to the
//  * BQ27Z7x gauge. It initializes the MSPM0 peripherals, communicates with
//  * the gauge over I2C and prints / blinks based on key gauging results.
//  ******************************************************************************
//  */

// #include "ti/devices/msp/peripherals/hw_i2c.h"
// #include <sys/_stdint.h>
// #include "ti_msp_dl_config.h"
// #include <stdbool.h>
// #include <stdio.h>

// #include "BQ27Z7.h"
// #include "BQ27Z7_functions.h"
// #include "BQ7690x_i2c.h"

// int main(void) {

//   /* Power on GPIO, initialize pins as digital outputs, I2C, etc. */
//   SYSCFG_DL_init();

//   /* Init I2C IRQ */
//   DL_SYSCTL_disableSleepOnExit();
//   delay_cycles(300);
//   NVIC_EnableIRQ(I2C_INST_INT_IRQN);
//   DL_I2C_enableControllerClockStretching(I2C_INST);

//   /* Default: LED1 and LED3 ON, LED2 OFF */
//   DL_GPIO_clearPins(GPIO_LEDS_PORT, GPIO_LEDS_USER_LED_2_PIN);
//   DL_GPIO_setPins(GPIO_LEDS_PORT,
//                   GPIO_LEDS_USER_LED_1_PIN | GPIO_LEDS_USER_LED_3_PIN);

//   printf("\nStarting BQ27Z7x gauge test sequence..\n\n");

//   /* Read a first snapshot of basic measurements and gauging data */
//   BQ27Z7_ReadBasicMeasurements();
//   BQ27Z7_ReadGauging();
//   BQ27Z7_ReadStatus();

//   printf("Pack Voltage      = %4i mV\n",  BQ27Z7_Get_Voltage_mV());
//   printf("Current           = %4i mA\n",  BQ27Z7_Get_Current_mA());
//   printf("Avg Current       = %4i mA\n",  BQ27Z7_Get_AvgCurrent_mA());
//   printf("Temperature       = %4i C\n",   BQ27Z7_Get_Temperature_C());
//   printf("Internal Temp     = %4i C\n",   BQ27Z7_Get_InternalTemperature_C());
//   printf("Avg Power         = %4i mW\n",  BQ27Z7_Get_AvgPower_mW());

//   printf("RemainingCapacity = %4u mAh\n", BQ27Z7_Get_RemainingCapacity_mAh());
//   printf("FullChargeCap     = %4u mAh\n", BQ27Z7_Get_FullChargeCapacity_mAh());
//   printf("DesignCapacity    = %4u mAh\n", BQ27Z7_Get_DesignCapacity_mAh());
//   printf("RSOC              = %4u %%\n",  BQ27Z7_Get_RSOC_percent());
//   printf("SOH               = %4u %%\n",  BQ27Z7_Get_SOH_percent());
//   printf("Cycle Count       = %4u\n",     BQ27Z7_Get_CycleCount());
//   printf("AvgTimeToEmpty    = %4u min\n", BQ27Z7_Get_AvgTimeToEmpty_min());
//   printf("AvgTimeToFull     = %4u min\n", BQ27Z7_Get_AvgTimeToFull_min());
//   printf("MaxLoadCurrent    = %4u mA\n",  BQ27Z7_Get_MaxLoadCurrent_mA());
//   printf("MaxLoadTTE        = %4u min\n", BQ27Z7_Get_MaxLoadTimeToEmpty_min());

//   printf("BatteryStatus     = 0x%04X\n",  BQ27Z7_Get_BatteryStatus());
//   printf("ControlStatus     = 0x%04X\n",  BQ27Z7_Get_ControlStatus());
//   printf("OperationStatus   = 0x%04X\n",  BQ27Z7_Get_OperationStatus());
//   printf("ChargingStatus    = 0x%04X\n",  BQ27Z7_Get_ChargingStatus());

//   printf("\nBQ27Z7x test sequence completed. Entering LED blink loop.\n");

//   /* Main loop - periodically refresh gauge data and toggle LEDs */
//   while (true) {

//     /* Periodically refresh basic measurements and RSOC */
//     BQ27Z7_ReadBasicMeasurements();
//     BQ27Z7_ReadGauging();

//     printf("\nSnapshot: V = %4i mV, I = %4i mA, RSOC = %3u %%\n",
//            BQ27Z7_Get_Voltage_mV(),
//            BQ27Z7_Get_Current_mA(),
//            BQ27Z7_Get_RSOC_percent());

//     /* Simple visual heartbeat */
//     delay_cycles(10000000);
//     DL_GPIO_togglePins(GPIO_LEDS_PORT,
//                        GPIO_LEDS_USER_LED_1_PIN |
//                        GPIO_LEDS_USER_LED_2_PIN |
//                        GPIO_LEDS_USER_LED_3_PIN);
//   }
// }

