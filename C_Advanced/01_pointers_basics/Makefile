TARGET = pointers_basics
SRC    = main.c
CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LIBS   = -lm

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

cross:
	arm-linux-gnueabihf-gcc $(CFLAGS) -o $(TARGET)-bbb $(SRC) $(LIBS)

clean:
	rm -f $(TARGET) $(TARGET)-bbb

.PHONY: all cross clean
