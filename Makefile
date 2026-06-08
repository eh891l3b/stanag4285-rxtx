# STANAG 4285 Standalone Modem
# Requires: gcc, libm, libpthread
# miniaudio.h must be present in this directory (single-header library)

CC      = gcc
CFLAGS  = -O2
LDFLAGS = -lm -lpthread

TARGET  = stanag4285

SRCS = autoprobe.c   \
       bstuff.c      \
       cas.c         \
       control.c     \
       convolutional.c \
       crc.c         \
       demodulate.c  \
       eqnew.c       \
       frame.c       \
       general.c     \
       interleaver.c \
       io_queue.c    \
       io_stdio.c    \
       kalman.c      \
       main.c        \
       receive.c     \
       sm_stub.c     \
       tables.c      \
       transmit.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# miniaudio is a large single-header; compile it separately without -Wall
main.o: main.c
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

# ---- Quick loopback tests ----
test: $(TARGET)
	@echo "=== Loopback tests ==="
	@for mode in 600s 1200s 2400s 2400l 3600u; do \
	  msg="TEST $$mode OK"; \
	  echo "$$msg" | ./$(TARGET) -tx -mode $$mode -wav /tmp/_lb_$$mode.wav 2>/dev/null; \
	  if ./$(TARGET) -rx -mode $$mode -wav /tmp/_lb_$$mode.wav 2>/dev/null | strings | grep -qF "$$msg"; then \
	    echo "PASS  $$mode"; \
	  else \
	    echo "FAIL  $$mode  expected: $$msg"; \
	  fi; \
	done
	@echo "=== Done ==="
