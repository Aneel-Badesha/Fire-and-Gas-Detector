.PHONY: all clean

BUILDDIR = build
CFLAGS = -D _POSIX_C_SOURCE=200809L -O2 -std=c17 -Wall -Werror -Wvla -Wextra
LDFLAGS = -pthread -lm

# Use cross-compiler if available else native gcc
CROSS := $(shell which arm-linux-gnueabihf-gcc 2>/dev/null)
ifneq ($(CROSS),)
	CC = arm-linux-gnueabihf-gcc
else
	CC = gcc
endif

OBJS = $(BUILDDIR)/main.o $(BUILDDIR)/sensor.o $(BUILDDIR)/common.o \
       $(BUILDDIR)/output.o $(BUILDDIR)/user.o $(BUILDDIR)/watchdog.o

all: $(BUILDDIR)/main

$(BUILDDIR)/main: $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/main.o: main.c sensor.h user.h common.h output.h watchdog.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/sensor.o: sensor.c sensor.h common.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/common.o: common.c common.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/output.o: output.c output.h common.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/user.o: user.c user.h common.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/watchdog.o: watchdog.c watchdog.h common.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
	rm -f *.o main
