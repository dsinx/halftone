CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = halftone
SRC = halftone.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)

clean_saidas:
	rm -f saidas/*.pgm

.PHONY: all clean clean_saidas
