################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../libraries/drivers/drv_adc.c \
../libraries/drivers/drv_gpio.c \
../libraries/drivers/drv_hwtimer.c \
../libraries/drivers/drv_i2c.c \
../libraries/drivers/drv_mcan.c \
../libraries/drivers/drv_rtc.c \
../libraries/drivers/drv_spi.c \
../libraries/drivers/drv_uart_v2.c 

OBJS += \
./libraries/drivers/drv_adc.o \
./libraries/drivers/drv_gpio.o \
./libraries/drivers/drv_hwtimer.o \
./libraries/drivers/drv_i2c.o \
./libraries/drivers/drv_mcan.o \
./libraries/drivers/drv_rtc.o \
./libraries/drivers/drv_spi.o \
./libraries/drivers/drv_uart_v2.o 

C_DEPS += \
./libraries/drivers/drv_adc.d \
./libraries/drivers/drv_gpio.d \
./libraries/drivers/drv_hwtimer.d \
./libraries/drivers/drv_i2c.d \
./libraries/drivers/drv_mcan.d \
./libraries/drivers/drv_rtc.d \
./libraries/drivers/drv_spi.d \
./libraries/drivers/drv_uart_v2.d 


# Each subdirectory must supply rules for building sources it contributes
libraries/drivers/%.o: ../libraries/drivers/%.c
	riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -mno-strict-align -mno-save-restore -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -gdwarf-2 -DDEBUG -DUSE_NONVECTOR_MODE=1 -DFLASH_XIP=1 -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\applications" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\board" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\drivers" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch\riscv\l1c" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\debug_console" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\dma_mgr" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\touch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\usb" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\drivers\inc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280\boot" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\ip" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\utils" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_interrupt_util" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_os_tick" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\spi" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\finsh" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\legacy" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\common\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\newlib" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\epoll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\eventfd" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\poll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\ipc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\libcpu\risc-v\common" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\startup\HPM6280" -include"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rtconfig_preinc.h" -std=gnu11 -ffunction-sections -fdata-sections -fno-common   -mcmodel=medlow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

