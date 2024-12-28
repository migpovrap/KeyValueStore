CC = gcc-14

CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L -I. \
		 -Wall -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
		 -pthread -fsanitize=thread -fsanitize=undefined


ifneq ($(shell uname -s),Darwin) # if not macOS
	CFLAGS += -fmax-errors=5
endif

all: server/kvs client/client

server/kvs: common/protocol.h common/constants.h server/main.c server/operations.o server/kvs.o server/io.o server/parser.o common/io.o server/notifications.o server/connections.o server/jobs_manager.o
	$(CC) $(CFLAGS) $(SLEEP) -o $@ $^

client/client: common/protocol.h common/constants.h client/main.c client/api.o client/parser.o common/io.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c} -o $@

clean:
	@rm -f common/*.o client/*.o server/*.o server/core/*.o server/kvs client/client client/client_write

rm:
	@rm -f server/jobs/*.bck server/jobs/*.out ./*.pipe

test: server/kvs client/client
	stdbuf -oL ./server/kvs ./server/jobs 4 2 ./teste.pipe | sed "s/^/\x1b[32m[SERVER]\x1b[0m /" &
	@sleep 0.002
	stdbuf -oL ./client/client 1 ./teste.pipe < ./tests/client_tests.txt | sed "s/^/\x1b[34m[CLIENT]\x1b[0m /"
	@sleep 0.002
	@$(MAKE) -s clean

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i common/*.c common/*.h client/*.c client/*.h server/*.c server/*.h
