CC = gcc
CFLAGS = -Wall -Wno-missing-field-initializers -Wextra -O2 $(shell pkg-config --cflags sdl3 sdl3-image sdl3-ttf) -Iinclude
LIBS = $(shell pkg-config --libs sdl3 sdl3-image sdl3-ttf)

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = expe3000

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
