# This Makefile is GNU make compatible. You can get GNU Make from
# http://gnuwin32.sourceforge.net/packages/make.htm

CCFLAGS = -Wall -c

ifeq ($(OS),Windows_NT)
	os = windows
	CCFLAGS += -D WIN32
	LDFLAGS += -l rasapi32 -l wininet
	ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
		CCFLAGS += -D AMD64
	endif
	ifeq ($(PROCESSOR_ARCHITECTURE),x86)
		CCFLAGS += -D IA32
	endif
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		os = linux
		CCFLAGS += -D LINUX $(shell pkg-config --cflags gio-2.0)
		LDFLAGS += $(shell pkg-config --libs gio-2.0)
	endif
	ifeq ($(UNAME_S),Darwin)
		os = darwin
		CCFLAGS += -D DARWIN -x objective-c
		LDFLAGS += -framework Cocoa -framework SystemConfiguration -framework Security
	endif
	UNAME_P := $(shell uname -p)
	ifeq ($(UNAME_P),x86_64)
		CCFLAGS += -D AMD64
	endif
	ifneq ($(filter %86,$(UNAME_P)),)
		CCFLAGS += -D IA32
	endif
	ifneq ($(filter arm%,$(UNAME_P)),)
		CCFLAGS += -D ARM
	endif
endif

CC=gcc

all: pac_$(os)
main.o: main.c common.h
	$(CC) $(CCFLAGS) $^
$(os).o: $(os).c common.h
	$(CC) $(CCFLAGS) $^
pac_$(os): $(os).o main.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm *.o pac_$(os)
