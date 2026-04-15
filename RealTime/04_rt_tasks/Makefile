TARGET = rt_tasks
SRC    = main.c
CC     = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu99
LIBS   = -lpthread -lrt

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

cross:
	arm-linux-gnueabihf-gcc $(CFLAGS) -o $(TARGET)-bbb $^ $(LIBS)

clean:
	rm -f $(TARGET) $(TARGET)-bbb

.PHONY: all cross clean
