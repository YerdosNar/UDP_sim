CC      := gcc
CFLAGS  := -Wall -Wextra -Wpedantic -std=c11 -g -Iinclude

SRC     := src
OBJ     := obj
BIN     := bin

COMMON  := $(OBJ)/packet.o $(OBJ)/transfer.o

all: $(BIN)/server $(BIN)/peer

test-drop: clean
	$(MAKE) CFLAGS="$(CFLAGS) -DDROP_TEST" all

$(BIN)/server: $(OBJ)/server.o $(COMMON) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN)/peer:   $(OBJ)/peer.o   $(COMMON) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ) $(BIN):
	mkdir -p $@

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all clean test-drop
