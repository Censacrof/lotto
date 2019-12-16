CC=gcc
CFLAGS=-Wall -g
SRC_DIR=./src
BIN_DIR=./bin
TEST_DIR=./test

SRC_ALL:=$(wildcard $(SRC_DIR)/*.c)											# aggiungo ogni file .c dentro src
SRC_CLIENT:=$(filter-out $(SRC_DIR)/server.c, $(SRC_ALL))					# tolgo server.c dai sorgenti del client
SRC_SERVER:=$(filter-out $(SRC_DIR)/client.c, $(SRC_ALL))					# tolgo client.c dai sorgenti del server
SRC_TEST:=$(wildcard $(TEST_DIR)/*.c)										# aggiungo ogni file .c dentro test

# sostituisco: src -> bin; .c -> .o; 
BIN_ALL:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_ALL:.c=.o))
BIN_CLIENT:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_CLIENT:.c=.o))
BIN_SERVER:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_SERVER:.c=.o))
BIN_TEST:=$(patsubst $(TEST_DIR)/%, $(TEST_DIR)/bin/%, $(SRC_TEST:.c=))

all: lotto-client lotto-server

lotto-client: $(BIN_CLIENT)
	$(CC) $(CFLAGS) $< -o $@

lotto-server: $(BIN_SERVER)
	$(CC) $(CFLAGS) $< -o $@

test: $(BIN_TEST)

# per ogni file .c contenuto in src/ aggiungo una ricetta per compilarlo
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# per ogni test aggiungo una regola per compilarlo
$(TEST_DIR)/bin/%: $(TEST_DIR)/%.c
	@mkdir -p $(TEST_DIR)/bin
	$(CC) -pthread $(CFLAGS) $< -o $@

.PHONY: clean	# clean non è un file
clean:
	rm -rf *.o $(TEST_DIR)/bin $(BIN_DIR) lotto-client lotto-server