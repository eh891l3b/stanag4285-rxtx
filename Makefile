CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter -std=c11 \
          -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -ldl -lpthread

TARGET  = stanag4285
SRCS    = main.c \
          control.c \
          transmit.c \
          receive.c \
          convolutional.c \
          demodulate.c \
          eqnew.c \
          interleaver.c \
          kalman.c \
          tables.c

OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
