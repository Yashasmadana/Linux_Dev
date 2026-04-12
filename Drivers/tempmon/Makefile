# TempMon Makefile
# Usage:
#   make              - native build (Linux / WSL)
#   make win          - Windows .exe via MSYS2 (run in CMD)
#   make cross        - cross-compile for BeagleBone Black (ARM)
#   make deploy IP=192.168.7.2  - scp binary to BBB
#   make clean

TARGET      = tempmon
SRC         = main.c

# Native compiler (Linux / WSL)
CC          ?= gcc
CFLAGS      = -Wall -Wextra -O2 -std=c99 -Wno-unused-result
LIBS        = -lncurses -lm

# Windows build via MSYS2 UCRT64
WIN_CC      = gcc
WIN_INC     = -I/c/msys64/ucrt64/include/ncurses
WIN_CFLAGS  = -Wall -Wextra -O2 -std=c99 -Wno-unused-result $(WIN_INC)
WIN_LIBS    = -L/c/msys64/ucrt64/lib -lncursesw -lm

# Cross-compiler for BBB (ARM Cortex-A8, hard-float)
CROSS_CC    = arm-linux-gnueabihf-gcc
CROSS_FLAGS = -Wall -Wextra -O2 -std=c99 -Wno-unused-result -march=armv7-a -mfpu=neon -mfloat-abi=hard
CROSS_LIBS  = -lncurses -lm

# BBB deploy target
BBB_USER    ?= debian
BBB_IP      ?= 192.168.7.2
BBB_DIR     ?= /home/debian/tempmon

# ----------------------------------------------------------------

.PHONY: all win cross deploy clean run help

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "Built native: ./$(TARGET)"

win: $(SRC)
	$(WIN_CC) $(WIN_CFLAGS) -o $(TARGET).exe $^ $(WIN_LIBS)
	@echo "Built Windows exe: ./$(TARGET).exe"

cross: $(SRC)
	$(CROSS_CC) $(CROSS_FLAGS) -o $(TARGET)-bbb $^ $(CROSS_LIBS)
	@echo "Built BBB binary: ./$(TARGET)-bbb"
	@file $(TARGET)-bbb

deploy: cross
	@echo "Deploying to $(BBB_USER)@$(BBB_IP):$(BBB_DIR)"
	ssh $(BBB_USER)@$(BBB_IP) "mkdir -p $(BBB_DIR)"
	scp $(TARGET)-bbb $(BBB_USER)@$(BBB_IP):$(BBB_DIR)/$(TARGET)
	@echo "Run on BBB:  ssh $(BBB_USER)@$(BBB_IP) $(BBB_DIR)/$(TARGET)"

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET)-bbb

help:
	@echo "Targets:"
	@echo "  make            Build native (for testing on host)"
	@echo "  make cross      Cross-compile for BBB"
	@echo "  make deploy     Cross-compile + scp to BBB"
	@echo "    IP=x.x.x.x   Override BBB IP (default: 192.168.7.2)"
	@echo "  make run        Build + run natively"
	@echo "  make clean      Remove binaries"
	@echo ""
	@echo "BBB setup:"
	@echo "  sudo apt install ncurses-dev              (on BBB)"
	@echo "  sudo insmod lm75_temp_sensor.ko           (load driver)"
	@echo "  echo lm75 0x48 | sudo tee /sys/bus/i2c/devices/i2c-1/new_device"
