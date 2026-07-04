CC = gcc
CFLAGS = -Wall -O2 -I./src
LDFLAGS = -lbluetooth -lgpiod

SRC = \
	src/main.c \
	src/rfcomm_epoll.c \
	src/app_hsm.c \
	src/gpio.c \
	src/crc16.c \
	src/log.c

OBJ = $(SRC:.c=.o)

TARGET = blt_hsm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

