CFLAGS = -Wall -Wpedantic -Werror -std=c17

SRC = ./src
BIN = ./bin

.PHONY: clean

sysholler: $(SRC)/sysholler.c
	$(CC) $(CFLAGS) $? -o $(BIN)/$@

debug: CFLAGS += -g
debug: sysholler

clean:
	$(RM) $(BIN)/*

$(shell mkdir -p $(BIN))