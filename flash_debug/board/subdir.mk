################################################################################
# 自动生成的文件。不要编辑！
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../board/board.c \
../board/fal_flash_port.c \
../board/pinmux.c \
../board/rtt_board.c 

OBJS += \
./board/board.o \
./board/fal_flash_port.o \
./board/pinmux.o \
./board/rtt_board.o 

C_DEPS += \
./board/board.d \
./board/fal_flash_port.d \
./board/pinmux.d \
./board/rtt_board.d 


# Each subdirectory must supply rules for building sources it contributes
board/%.o: ../board/%.c
	riscv32-unknown-elf-gcc -march=rv32imac -mabi=ilp32 -msmall-data-limit=8 -mno-strict-align -mno-save-restore -Og -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections  -g3 -gdwarf-2 -DDEBUG -DUSE_NONVECTOR_MODE=1 -DFLASH_XIP=1 -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\applications" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\board" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\drivers" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch\riscv\l1c" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\arch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\debug_console" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\dma_mgr" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\touch" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\components\usb" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\drivers\inc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280\boot" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\HPM6280" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\soc\HPM6200\ip" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\hpm_sdk\utils" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_interrupt_util" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\libraries\misc\rtt_os_tick" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\drivers\spi" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\finsh" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\legacy" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\common\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\compilers\newlib" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\epoll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\eventfd" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\io\poll" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\components\libc\posix\ipc" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\include" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rt-thread\libcpu\risc-v\common" -I"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\startup\HPM6280" -include"D:\Users\lyx\project\0407_CARDVR\1\hpm6200SDKv1.10_tachographs_isok\rtconfig_preinc.h" -std=gnu11 -ffunction-sections -fdata-sections -fno-common   -mcmodel=medlow -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

