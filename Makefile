.POSIX:
CC     = cc
CFLAGS = -Wall -Werror -Wextra -O3

ifeq ($(OS), Windows_NT)
	LDFLAGS = -s -static
	LDLIBS  = -ladvapi32
	EXE     = .exe
else
	LDFLAGS =
	LDLIBS  =
endif

all: key_exchange$(EXE)

key_exchange$(EXE): *.c
	$(CC) $(CFLAGS) *.c -o $@ $(LDLIBS) $(LDFLAGS)
