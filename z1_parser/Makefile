CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra
TARGET  = parser
SRC     = parser_template.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
