SHELL := /bin/bash
CC = gcc

CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L -O3 \
  -Wall -Werror -Wextra \
  -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
  -fsanitize=address -fsanitize=undefined

VALGRIND_CFLAGS = -g -O3 -std=c17 -D_POSIX_C_SOURCE=200809L \
  -Wall -Werror -Wextra \
  -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
  -lpthread

ifneq ($(shell uname -s),Darwin) # if not macOS
	CFLAGS += -fmax-errors=5
endif

all: kvs
	@if [ ! -f ../lizard_output.log ]; then \
		lizard -L 50 -T nloc=50 -m *.c | tee ../lizard_output.log; \
	fi

kvs: main.c constants.h operations.o parser.o kvs.o jobs_parser.o
	@$(CC) $(CFLAGS) $(SLEEP) -o kvs main.c operations.o parser.o kvs.o jobs_parser.o -lpthread

%.o: %.c %.h
	@$(CC) $(CFLAGS) -c ${@:.o=.c}

run: kvs
	@export MallocNanoZone=0 && ./kvs ../jobs 7 3

rm: # Removes job outs and backups.
	@rm -rf ../jobs/*.out ../jobs/**/*.out
	@rm -rf ../jobs/*.bck ../jobs/**/*.bck
	@rm -f ../tests-public/jobs/*.out
	@rm -f ../tests-public/jobs/*.bck
	@rm -f ../tests-public/jobs2/*/*.out
	@rm -f ../tests-public/jobs2/*/*.bck
	@rm -f ../lizard_output.log

clean:
	@rm -f ../tests-public/kvs
	@rm -rf kvs.dSYM
	@rm -f *.o kvs

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	@clang-format -i *.c *.h

test_private: kvs
	@export MallocNanoZone=0 && time ./kvs ../jobs 79 456
	@echo "Done"
	@$(MAKE) -s clean

test_public: kvs
	@cp ./kvs ../tests-public/kvs
	@../tests-public/run_ex1.sh kvs
	@../tests-public/run_ex2.sh kvs
	@$(MAKE) -s clean

valgrind: CFLAGS=$(VALGRIND_CFLAGS)
valgrind: kvs
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes ./kvs ../jobs 7 3
	@$(MAKE) -s clean

stress_test: kvs
	@for i in {1..50}; do \
		export MallocNanoZone=0 && ./kvs ../jobs 200 150; \
		$(MAKE) -s rm; \
		echo "Run $$i completed"; \
	done
	@$(MAKE) -s clean
