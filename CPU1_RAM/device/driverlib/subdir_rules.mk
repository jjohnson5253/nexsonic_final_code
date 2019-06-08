################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
device/driverlib/%.obj: ../device/driverlib/%.c $(GEN_OPTS) | $(GEN_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: C2000 Compiler'
	"C:/ti/ccsv8/tools/compiler/ti-cgt-c2000_18.1.3.LTS/bin/cl2000" -v28 -ml -mt --cla_support=cla2 --float_support=fpu32 --tmu_support=tmu0 --vcu_support=vcu0 -Ooff --include_path="C:/ti/c2000/C2000Ware_1_00_06_00/driverlib/f28004x/driverlib" --include_path="C:/Users/jake/workspace_v8/main_code_python_gui_hr" --include_path="C:/ti/c2000/C2000Ware_1_00_06_00/device_support/f28004x/common/include" --include_path="C:/ti/c2000/C2000Ware_1_00_06_00/device_support/f28004x/headers/include" --include_path="C:/ti/c2000/C2000Ware_1_00_06_00/libraries/calibration/hrpwm/f28004x/include" --include_path="C:/ti/ccsv8/tools/compiler/ti-cgt-c2000_18.1.3.LTS/include" --define=CPU1 --diag_suppress=10063 --diag_warning=225 --diag_wrap=off --display_error_number --preproc_with_compile --preproc_dependency="device/driverlib/$(basename $(<F)).d_raw" --obj_directory="device/driverlib" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


