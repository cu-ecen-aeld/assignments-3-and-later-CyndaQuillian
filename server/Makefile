CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET = aesdsocket
LDFLAGS ?= 

all: $(TARGET)
default: $(TARGET)

$(TARGET): aesdsocket.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o