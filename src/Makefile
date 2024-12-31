CC = gcc-14

CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L -I. \
		 -Wall -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
		 -pthread

ifneq ($(shell uname -s),Darwin) # if not macOS
	CFLAGS += -fmax-errors=5
	SHELL := /bin/bash
endif

all: server/kvs client/client

server/kvs: common/protocol.h common/constants.h server/main.c server/operations.o server/kvs.o server/io.o server/parser.o common/io.o server/notifications.o server/connections.o server/jobs_manager.o server/utils.o
	@$(CC) $(CFLAGS) $(SLEEP) -o $@ $^

client/client: common/protocol.h common/constants.h client/main.c client/api.o client/utils.o client/parser.o common/io.o
	@$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c ${@:.o=.c} -o $@

clean:
	@rm -f common/*.o client/*.o server/*.o server/core/*.o server/kvs client/client client/client_write ./*.pipe

rm:
	@rm -f server/jobs/*.bck server/jobs/*.out ./*.pipe

test: test1 test2 test3

test1: server/kvs client/client
	@echo "Running test 1:"
	@stdbuf -oL ./server/kvs ./server/jobs 2 2 ./teste.pipe | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" & echo $$! > server_pid.tmp
	@sleep 0.5
	@stdbuf -oL ./client/client 1 ./teste.pipe < ./tests/client_tests.txt | sed "s/^/\x1b[34m[CLIENT_1]\x1b[0m /" &
	@sleep 0.5
	@stdbuf -oL ./client/client 2 ./teste.pipe | sed "s/^/\x1b[35m[CLIENT_2]\x1b[0m /" &
	@sleep 3
	@kill -s SIGINT $$(cat server_pid.tmp)
	@sleep 2
	@rm -f server_pid.tmp

test2: server/kvs client/client
	@echo "Running test 2:"
	@stdbuf -oL ./server/kvs ./server/jobs 4 3 ./teste.pipe | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" & echo $$! > server_pid.tmp
	@sleep 0.5
	@stdbuf -oL ./client/client 1 ./teste.pipe < ./tests/client_tests.txt | sed "s/^/\x1b[34m[CLIENT_1]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 2 ./teste.pipe | sed "s/^/\x1b[35m[CLIENT_2]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 3 ./teste.pipe | sed "s/^/\x1b[36m[CLIENT_3]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 4 ./teste.pipe | sed "s/^/\x1b[31m[CLIENT_4]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 5 ./teste.pipe | sed "s/^/\x1b[33m[CLIENT_5]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 6 ./teste.pipe | sed "s/^/\x1b[37m[CLIENT_6]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 7 ./teste.pipe | sed "s/^/\x1b[90m[CLIENT_7]\x1b[0m /" &
	@sleep 0.2
	@stdbuf -oL ./client/client 8 ./teste.pipe | sed "s/^/\x1b[91m[CLIENT_8]\x1b[0m /" &
	@sleep 5
	@kill -s SIGINT $$(cat server_pid.tmp)
	@sleep 3
	@rm -f server_pid.tmp

test3: server/kvs client/client
	@echo "Running test 3:"
	@stdbuf -oL ./server/kvs ./server/jobs 2 2 ./teste.pipe | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" & echo $$! > server_pid.tmp
	@sleep 1
	@stdbuf -oL ./client/client 1 ./teste.pipe < ./tests/client_tests2.txt | sed "s/^/\x1b[34m[CLIENT_1]\x1b[0m /" &
	@sleep 4
	@kill -s SIGINT $$(cat server_pid.tmp)
	@sleep 1
	@rm -f server_pid.tmp
	@$(MAKE) -s clean

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i common/*.c common/*.h client/*.c client/*.h server/*.c server/*.h
