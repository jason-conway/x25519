.POSIX:
CC     = cc
CFLAGS = -Wall -Werror -Wextra -O3
BUILD_DIR = ./build/
MKDIR = mkdir -p

ifeq ($(OS), Windows_NT)
	LDFLAGS = -s -static
	LDLIBS  = -ladvapi32
	EXE     = .exe
else
	LDFLAGS =
	LDLIBS  =
endif

all: key_exchange$(EXE)

key_exchange$(EXE): x25519.* key_exchange.c
	$(MKDIR) $(BUILD_DIR)
	$(CC) $(CFLAGS) *.c -o $(BUILD_DIR)$@ $(LDLIBS) $(LDFLAGS)
