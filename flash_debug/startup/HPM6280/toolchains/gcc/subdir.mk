################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_UPPER_SRCS += \
../startup/HPM6280/toolchains/gcc/port_gcc.S \
../startup/HPM6280/toolchains/gcc/start.S 

OBJS += \
./startup/HPM6280/toolchains/gcc/port_gcc.o \
./startup/HPM6280/toolchains/gcc/start.o 

S_UPPER_DEPS += \
./startup/HPM6280/toolchains/gcc/port_gcc.d \
./startup/HPM6280/toolchains/gcc/start.d 


# Each subdirectory must supply rules for building sources it contributes
startup/HPM6280/toolchains/gcc/%.o: ../startup/HPM6280/toolchains/gcc/%.S
	riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -mno-strict-align -mno-save-restore -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -gdwarf-2 -x assembler-with-cpp -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\ip" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\libcpu\risc-v\common" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\startup\HPM6280" -ffunction-sections -fdata-sections -fno-common -mcmodel=medlow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

