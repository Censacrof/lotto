CC=gcc
CFLAGS=-Wall -g -std=gnu90
LINK_CLIENT=
LINK_SERVER=-lcrypt
SRC_DIR=./src
BIN_DIR=./bin

SRC_COMMON:=$(wildcard $(SRC_DIR)/*.c)
SRC_CLIENT:=$(SRC_COMMON) $(wildcard $(SRC_DIR)/client/*.c) $(wildcard $(SRC_DIR)/client/**/*.c)
SRC_SERVER:=$(SRC_COMMON) $(wildcard $(SRC_DIR)/server/*.c) $(wildcard $(SRC_DIR)/server/**/*.c)

# sostituisco: src -> bin; .c -> .o; 
BIN_COMMON:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_COMMON:.c=.o))
BIN_CLIENT:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_CLIENT:.c=.o))
BIN_SERVER:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_SERVER:.c=.o))

.PHONY: all	# all non è un file
all: client server

.PHONY: client	# client non è un file
client: lotto_client

.PHONY: server	# server non è un file
server: lotto_server

lotto_client: $(BIN_CLIENT)
	$(CC) $(CFLAGS) $^ -o $@ $(LINK_CLIENT)

lotto_server: $(BIN_SERVER)
	$(CC) $(CFLAGS) $^ -o $@ $(LINK_SERVER)

# per ogni file .c contenuto in src/ aggiungo una ricetta per compilarlo
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean # clean non è un file
clean:
	rm -rf *.o $(BIN_DIR) lotto_client lotto_server

.PHONY: clean-data
clean-data:
	rm -rf data/