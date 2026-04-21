################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../libraries/hpm_sdk/drivers/src/hpm_adc16_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_dma_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_gpio_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_gptmr_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_i2c_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_mcan_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_pcfg_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_pllctlv2_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_pmp_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_rtc_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_spi_drv.c \
../libraries/hpm_sdk/drivers/src/hpm_uart_drv.c 

OBJS += \
./libraries/hpm_sdk/drivers/src/hpm_adc16_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_dma_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_gpio_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_gptmr_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_i2c_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_mcan_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_pcfg_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_pllctlv2_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_pmp_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_rtc_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_spi_drv.o \
./libraries/hpm_sdk/drivers/src/hpm_uart_drv.o 

C_DEPS += \
./libraries/hpm_sdk/drivers/src/hpm_adc16_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_dma_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_gpio_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_gptmr_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_i2c_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_mcan_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_pcfg_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_pllctlv2_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_pmp_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_rtc_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_spi_drv.d \
./libraries/hpm_sdk/drivers/src/hpm_uart_drv.d 


# Each subdirectory must supply rules for building sources it contributes
libraries/hpm_sdk/drivers/src/%.o: ../libraries/hpm_sdk/drivers/src/%.c
	riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -mno-strict-align -mno-save-restore -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -gdwarf-2 -DDEBUG -DUSE_NONVECTOR_MODE=1 -DFLASH_XIP=1 -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\packages\u8g2-official-latest\csrc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\applications" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\board" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\drivers" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch\riscv\l1c" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\debug_console" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\dma_mgr" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\touch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\usb" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\drivers\inc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280\boot" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\ip" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\utils" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_interrupt_util" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_os_tick" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\spi" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\finsh" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\legacy" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\common\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\newlib" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\epoll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\eventfd" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\poll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\ipc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\libcpu\risc-v\common" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\startup\HPM6280" -include"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rtconfig_preinc.h" -std=gnu11 -ffunction-sections -fdata-sections -fno-common   -mcmodel=medlow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

