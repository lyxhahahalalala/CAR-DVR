################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../rt-thread/libcpu/risc-v/common/cpuport.c \
../rt-thread/libcpu/risc-v/common/trap_common.c 

S_UPPER_SRCS += \
../rt-thread/libcpu/risc-v/common/context_gcc.S \
../rt-thread/libcpu/risc-v/common/interrupt_gcc.S 

OBJS += \
./rt-thread/libcpu/risc-v/common/context_gcc.o \
./rt-thread/libcpu/risc-v/common/cpuport.o \
./rt-thread/libcpu/risc-v/common/interrupt_gcc.o \
./rt-thread/libcpu/risc-v/common/trap_common.o 

S_UPPER_DEPS += \
./rt-thread/libcpu/risc-v/common/context_gcc.d \
./rt-thread/libcpu/risc-v/common/interrupt_gcc.d 

C_DEPS += \
./rt-thread/libcpu/risc-v/common/cpuport.d \
./rt-thread/libcpu/risc-v/common/trap_common.d 


# Each subdirectory must supply rules for building sources it contributes
rt-thread/libcpu/risc-v/common/%.o: ../rt-thread/libcpu/risc-v/common/%.S
	riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -mno-strict-align -mno-save-restore -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -gdwarf-2 -x assembler-with-cpp -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\ip" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\libcpu\risc-v\common" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\startup\HPM6280" -ffunction-sections -fdata-sections -fno-common -mcmodel=medlow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
rt-thread/libcpu/risc-v/common/%.o: ../rt-thread/libcpu/risc-v/common/%.c
	riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -mno-strict-align -mno-save-restore -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -gdwarf-2 -DDEBUG -DUSE_NONVECTOR_MODE=1 -DFLASH_XIP=1 -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\applications" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\board" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\drivers" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch\riscv\l1c" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\debug_console" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\dma_mgr" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\touch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\usb" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\drivers\inc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280\boot" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\ip" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\utils" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_interrupt_util" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_os_tick" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\spi" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\finsh" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\legacy" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\common\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\newlib" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\epoll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\eventfd" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\poll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\ipc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\libcpu\risc-v\common" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\startup\HPM6280" -include"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rtconfig_preinc.h" -std=gnu11 -ffunction-sections -fdata-sections -fno-common   -mcmodel=medlow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

