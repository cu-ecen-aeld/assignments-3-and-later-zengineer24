#Used source to help develop this file: https://makefiletutorial.com/

USE_AESD_CHAR_DEVICE ?= 1
CFLAGS = -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE) -g -Wall -O0
CC ?= $(CROSS_COMPILE)gcc
#writer: writer.c
#	cc writer.c -o writer

#default target
all: aesdsocket

aesdsocket: aesdsocket.o

clean:
	rm -f aesdsocket aesdsocket.o
