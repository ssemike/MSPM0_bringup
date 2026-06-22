################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
sx126x_Ra-01S-P_Ra-01SH-P_Driver_V0.0.4/peripherals/radio/sx126x_example/sx126x_recive/%.o: ../sx126x_Ra-01S-P_Ra-01SH-P_Driver_V0.0.4/peripherals/radio/sx126x_example/sx126x_recive/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Arm Compiler: "$<"'
	"C:/ti/ccs2031/ccs/tools/compiler/ti-cgt-armllvm_4.0.4.LTS/bin/tiarmclang.exe" -c @"device.opt"  -march=thumbv6m -mcpu=cortex-m0plus -mfloat-abi=soft -mlittle-endian -mthumb -O0 -I"C:/Users/Admin/Desktop/HW_SW_interface_CCS" -I"C:/Users/Admin/Desktop/HW_SW_interface_CCS/Debug" -I"C:/ti/mspm0_sdk_2_09_00_01/source/third_party/CMSIS/Core/Include" -I"C:/ti/mspm0_sdk_2_09_00_01/source" -gdwarf-3 -MMD -MP -MF"sx126x_Ra-01S-P_Ra-01SH-P_Driver_V0.0.4/peripherals/radio/sx126x_example/sx126x_recive/$(basename $(<F)).d_raw" -MT"$(@)"  $(GEN_OPTS__FLAG) -o"$@" "$<"
	@echo ' '


