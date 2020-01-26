CC=gcc
CFLAGS=-Wall -g
LINK_CLIENT=
LINK_SERVER=-lcrypt
SRC_DIR=./src
BIN_DIR=./bin
TEST_DIR=./test

SRC_COMMON:=$(wildcard $(SRC_DIR)/*.c)
SRC_CLIENT:=$(SRC_COMMON) $(wildcard $(SRC_DIR)/client/*.c) $(wildcard $(SRC_DIR)/client/**/*.c)
SRC_SERVER:=$(SRC_COMMON) $(wildcard $(SRC_DIR)/server/*.c) $(wildcard $(SRC_DIR)/server/**/*.c)
SRC_TEST:=$(wildcard $(TEST_DIR)/*.c)

# sostituisco: src -> bin; .c -> .o; 
BIN_COMMON:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_COMMON:.c=.o))
BIN_CLIENT:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_CLIENT:.c=.o))
BIN_SERVER:=$(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(SRC_SERVER:.c=.o))
BIN_TEST:=$(patsubst $(TEST_DIR)/%, $(BIN_DIR)/test/%, $(SRC_TEST:.c=))

.PHONY: all	# all non è un file
all: lotto-client lotto-server

lotto-client: $(BIN_CLIENT)
	$(CC) $(CFLAGS) $^ -o $@ $(LINK_CLIENT)

lotto-server: $(BIN_SERVER)
	$(CC) $(CFLAGS) $^ -o $@ $(LINK_SERVER)

# per ogni file .c contenuto in src/ aggiungo una ricetta per compilarlo
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean # clean non è un file
clean:
	rm -rf *.o $(BIN_DIR) lotto-client lotto-server

.PHONY: clean-data
clean-data:
	rm -rf data/

# ----------------------------- TESTS -----------------------------
# unità di compilazione che non contengono definizioni di main
LIB_CLIENT:=$(filter-out $(BIN_DIR)/clinet/client.o $(BIN_COMMON), $(BIN_CLIENT))
LIB_SERVER:=$(filter-out $(BIN_DIR)/server/server.o $(BIN_COMMON), $(BIN_SERVER))
LIB_COMMON:=$(BIN_COMMON) # solo per leggibilità

# compila tutti i tests
test: $(BIN_TEST)

# testa le funzioni send_msg e recv_msg 
$(BIN_DIR)/test/msg: $(LIB_COMMON) test/msg.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -pthread $^ -o $@


# semplice client per testare il server (simile a netcat)
$(BIN_DIR)/test/trivial_client: $(LIB_COMMON) test/trivial_client.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@


# esperimenti con le regex
$(BIN_DIR)/test/regex: $(LIB_COMMON) test/regex.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@


# testa la serializzazione
$(BIN_DIR)/test/serialization: $(LIB_COMMON) test/serialization.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@ $(LINK_SERVER)