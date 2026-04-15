TARGET     = blink
SRC        = blink.c
ASM        = blink.asm
CC         = gcc
CFLAGS     = -Wall -Wextra -O2 -std=gnu99

# PRU assembler (install: sudo apt-get install am335x-pru-package)
PASM       = pasm
FW_DIR     = /lib/firmware

all: $(TARGET) firmware

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

cross:
	arm-linux-gnueabihf-gcc $(CFLAGS) -o $(TARGET)-bbb $(SRC)

firmware: $(ASM)
	@which $(PASM) > /dev/null 2>&1 && \
		$(PASM) -b $(ASM) || \
		echo "pasm not found — install: sudo apt-get install am335x-pru-package"

install-fw: firmware
	sudo cp blink.out $(FW_DIR)/blink_pru0.out
	@echo "Firmware installed to $(FW_DIR)/blink_pru0.out"

clean:
	rm -f $(TARGET) $(TARGET)-bbb blink.out

.PHONY: all cross firmware install-fw clean
