# Makefile for primGPT_F411 (STM32F411CEUx)
# This is a basic Makefile to allow command-line builds and fix include issues.

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Defines
DEFINES = -DUSE_HAL_DRIVER -DSTM32F411xE

# Include paths
INCLUDES = \
-I Core/Inc \
-I Drivers/STM32F4xx_HAL_Driver/Inc \
-I Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
-I Drivers/CMSIS/Device/ST/STM32F4xx/Include \
-I Drivers/CMSIS/Include \
-I Middlewares/Third_Party/FreeRTOS/Source/include \
-I Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
-I Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F

# Compilation flags
CFLAGS = $(MCU) $(DEFINES) $(INCLUDES) -O0 -g3 -Wall -fstack-usage -ffunction-sections -fdata-sections --specs=nano.specs

# Source files (Core only for verification, add more as needed)
SRCS = \
Core/Src/main.c \
Core/Src/freertos.c \
Core/Src/stm32f4xx_it.c \
Core/Src/stm32f4xx_hal_msp.c \
Core/Src/stm32f4xx_hal_timebase_tim.c \
Core/Src/system_stm32f4xx.c \
Core/Src/uart.c \
Core/Src/fram.c \
Core/Src/crc16.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean check

all: check

check: $(OBJS)
	@echo "Compilation successful. All Core files compiled with correct includes."

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
