SHELL := /bin/bash
CC = gcc

# Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L \
		 -Wall -Werror -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \

VALGRIND_CFLAGS = -g -O3 -std=c17 -D_POSIX_C_SOURCE=200809L \
  -Wall -Werror -Wextra \
  -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused -pthread

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: kvs

kvs: main.c constants.h operations.o parser.o kvs.o io.o notifications.o connections.o jobs_manager.o
	$(CC) $(CFLAGS) $(SLEEP) -o kvs main.c operations.o parser.o kvs.o io.o notifications.o connections.o jobs_manager.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c}

run: kvs
	@./kvs

rm: # Removes job outs and backups.
	@rm -rf ./jobs/*.out
	@rm -rf ./jobs/*.bck
	@rm -f ../lizard_output.log

clean:
	rm -f *.o kvs

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i *.c *.h

test: kvs
	@{ time ./kvs jobs 2 8 > /dev/null; } 2>&1 | grep -E 'real|user|sys'
	@$(MAKE) -s clean

valgrind: CFLAGS=$(VALGRIND_CFLAGS)
valgrind: kvs
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes ./kvs ../jobs 12 8
	@$(MAKE) -s clean

stress_test: kvs
	@for i in {1..50}; do \
		./kvs ../jobs 12 8; \
		$(MAKE) -s rm; \
		echo "Run $$i completed"; \
	done
	@$(MAKE) -s clean
