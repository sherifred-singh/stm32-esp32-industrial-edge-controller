# Master Makefile for STM32F446RE Real-Time Core Firmware
TARGET = stm32_edge_controller

# Toolchain definitions
CC      = arm-none-eabi-gcc
CP      = arm-none-eabi-objcopy
SZ      = arm-none-eabi-size
HEX     = $(CP) -O ihex
BIN     = $(CP) -O binary -S

# Build directory
BUILD_DIR = build

# C Sources
C_SOURCES = \
Core/Src/main.c \
Core/Src/sensor_acquisition.c \
Core/Src/actuator_control.c \
Core/Src/fault_supervisor.c \
Core/Src/uart_dma_comm.c \
../common/src/intermcu_protocol.c \
../common/src/intermcu_crc16.c

# C Includes
C_INCLUDES = \
-ICore/Inc \
-I../common/include

# MCU Compiler Flags (Cortex-M4 with FPU)
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

CFLAGS = $(MCU) $(C_INCLUDES) -O2 -Wall -fdata-sections -ffunction-sections -std=c99

# Phony targets
.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	@echo "Linking $(TARGET).elf"
	@$(CC) $(OBJECTS) $(MCU) -Wl,--gc-sections -o $@
	@$(SZ) $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
