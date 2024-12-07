SHELL := /bin/bash
CC = gcc

# Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L \
		 -Wall -Werror -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
		 -fsanitize=address -fsanitize=undefined

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: kvs

kvs: main.c constants.h operations.o parser.o kvs.o jobs_parser.o
	@$(CC) $(CFLAGS) $(SLEEP) -o kvs main.c operations.o parser.o kvs.o jobs_parser.o

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c ${@:.o=.c}

run: kvs
	@export MallocNanoZone=0 && ./kvs

rm:
	@rm -f ../jobs/*.out
	@rm -f ../jobs/*.bck
	@rm -f ../tests-public/jobs/*.out
	@rm -f ../tests-public/jobs/*.bck
	@rm -f ../tests-public/jobs2/*/*.out
	@rm -f ../tests-public/jobs2/*/*.bck
	@$(MAKE) -s clean

clean:
	@rm -f ../tests-public/kvs
	@rm -rf kvs.dSYM
	@rm -f *.o kvs

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	@clang-format -i *.c *.h

test_manual_private: kvs
	@export MallocNanoZone=0 && ./kvs ../jobs 1 1
	@$(MAKE) -s clean

test_public: kvs
	@cp ./kvs ../tests-public/kvs
	@../tests-public/run_ex1.sh kvs
	@../tests-public/run_ex2.sh kvs
	@$(MAKE) -s clean