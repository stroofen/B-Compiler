CC = gcc

CFLAGS = -std=gnu99 -O3 -Wall -Wextra

SRC_FILES := $(subst \,/,$(shell dir /s /b src\*.c))
OBJ_FILES := $(SRC_FILES:.c=.o)

TARGET = bc

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ_FILES) $(TARGET)

.PHONY: all clean