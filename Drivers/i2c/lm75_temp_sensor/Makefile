# Makefile for I2C Temperature Sensor Driver

# Module name (change this to match your .c filename without .c extension)
obj-m += main.o

# Kernel build directory
KDIR := /lib/modules/$(shell uname -r)/build

# Current directory
PWD := $(shell pwd)

# Default target - build the module
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean build files
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Install (load) module
install: all
	sudo insmod main.ko

# Uninstall (unload) module
uninstall:
	sudo rmmod main

# Show module info
info:
	modinfo main.ko

# Show kernel log
log:
	dmesg | tail -20

# Load module and check log
test: install log

# Help
help:
	@echo "Available targets:"
	@echo "  make         - Build the module"
	@echo "  make clean   - Remove build files"
	@echo "  make install - Load the module"
	@echo "  make uninstall - Unload the module"
	@echo "  make info    - Show module information"
	@echo "  make log     - Show kernel messages"
	@echo "  make test    - Build, load, and check log"

.PHONY: all clean install uninstall info log test help