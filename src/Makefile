CC = gcc

CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L -I. \
     -Wall -Wextra \
     -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
     -pthread

ifneq ($(shell uname -s),Darwin) # if not macOS
  CFLAGS += -fmax-errors=5
  SHELL := /bin/bash
endif

SERVER_SRC = server
CLIENT_SRC = client
COMMON_SRC = common
TEST_SRC = tests
PIPE = ./teste.pipe

SERVER_OBJS = $(SERVER_SRC)/operations.o $(SERVER_SRC)/kvs.o $(SERVER_SRC)/io.o $(SERVER_SRC)/parser.o $(COMMON_SRC)/io.o $(SERVER_SRC)/notifications.o $(SERVER_SRC)/connections.o $(SERVER_SRC)/jobs_manager.o $(SERVER_SRC)/utils.o
CLIENT_OBJS = $(CLIENT_SRC)/api.o $(CLIENT_SRC)/utils.o $(CLIENT_SRC)/parser.o $(COMMON_SRC)/io.o

all: $(SERVER_SRC)/kvs $(CLIENT_SRC)/client

$(SERVER_SRC)/kvs: $(COMMON_SRC)/protocol.h $(COMMON_SRC)/constants.h $(SERVER_SRC)/main.c $(SERVER_OBJS)
	@$(CC) $(CFLAGS) $(SLEEP) -o $@ $^

$(CLIENT_SRC)/client: $(COMMON_SRC)/protocol.h $(COMMON_SRC)/constants.h $(CLIENT_SRC)/main.c $(CLIENT_OBJS)
	@$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c ${@:.o=.c} -o $@

clean:
	@rm -f $(COMMON_SRC)/*.o $(CLIENT_SRC)/*.o $(SERVER_SRC)/*.o $(SERVER_SRC)/core/*.o $(SERVER_SRC)/kvs $(CLIENT_SRC)/client $(CLIENT_SRC)/client_write ./*.pipe

rm:
	@rm -f $(SERVER_SRC)/jobs/*.bck $(SERVER_SRC)/jobs/*.out $(PIPE)

test: test1 test2 test3

test1: $(SERVER_SRC)/kvs $(CLIENT_SRC)/client
	@echo "Running test 1:"
	@stdbuf -oL ./$(SERVER_SRC)/kvs ./$(SERVER_SRC)/jobs 2 2 $(PIPE) | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" & echo $$! > server_pid.tmp
	@sleep 0.5
	@stdbuf -oL ./$(CLIENT_SRC)/client 1 $(PIPE) < ./$(TEST_SRC)/client_tests.txt | sed "s/^/\x1b[34m[CLIENT_1]\x1b[0m /" &
	@sleep 0.5
	@stdbuf -oL ./$(CLIENT_SRC)/client 2 $(PIPE) | sed "s/^/\x1b[35m[CLIENT_2]\x1b[0m /" &
	@sleep 3
	@kill -s SIGINT $$(cat server_pid.tmp)
	@sleep 4
	@rm -f server_pid.tmp

test2: $(SERVER_SRC)/kvs $(CLIENT_SRC)/client
	@echo "Running test 2:"
	@stdbuf -oL ./$(SERVER_SRC)/kvs ./$(SERVER_SRC)/jobs 4 3 $(PIPE) | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" & echo $$! > server_pid.tmp
	@sleep 0.5
	@stdbuf -oL ./$(CLIENT_SRC)/client 1 $(PIPE) < ./$(TEST_SRC)/client_tests.txt | sed "s/^/\x1b[34m[CLIENT_1]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 2 $(PIPE) | sed "s/^/\x1b[35m[CLIENT_2]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 3 $(PIPE) | sed "s/^/\x1b[36m[CLIENT_3]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 4 $(PIPE) | sed "s/^/\x1b[31m[CLIENT_4]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 5 $(PIPE) | sed "s/^/\x1b[33m[CLIENT_5]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 6 $(PIPE) | sed "s/^/\x1b[37m[CLIENT_6]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 7 $(PIPE) | sed "s/^/\x1b[90m[CLIENT_7]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 7 $(PIPE) | sed "s/^/\x1b[91m[CLIENT_8]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 8 $(PIPE) | sed "s/^/\x1b[91m[CLIENT_8]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./$(CLIENT_SRC)/client 9 $(PIPE) | sed "s/^/\x1b[91m[CLIENT_8]\x1b[0m /" &
	@sleep 5
	@kill -s SIGINT $$(cat server_pid.tmp)
	@sleep 3
	@rm -f server_pid.tmp

test3: $(SERVER_SRC)/kvs $(CLIENT_SRC)/client
	@echo "Running test 3:"
	@stdbuf -oL ./$(SERVER_SRC)/kvs ./$(SERVER_SRC)/jobs 2 2 $(PIPE) | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" & echo $$! > server_pid.tmp
	@sleep 1
	@stdbuf -oL ./$(CLIENT_SRC)/client 1 $(PIPE) < ./$(TEST_SRC)/client_tests2.txt | sed "s/^/\x1b[34m[CLIENT_1]\x1b[0m /" &
	@sleep 4
	@kill -s SIGINT $$(cat server_pid.tmp)
	@sleep 3
	@rm -f server_pid.tmp
	@$(MAKE) -s clean

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i $(COMMON_SRC)/*.c $(COMMON_SRC)/*.h $(CLIENT_SRC)/*.c $(CLIENT_SRC)/*.h $(SERVER_SRC)/*.c $(SERVER_SRC)/*.h