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
	@rm -f ../jobs/*.diff
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

# Runs with a max of two forks and single threadhed
test_private: kvs
	@export MallocNanoZone=0 && ./kvs ../jobs 2 1 
	@passed_tests=0; \
	total_tests=0; \
	for input in $(wildcard ../jobs/*.job); do \
		total_tests=$$((total_tests + 1)); \
		output=$${input%.job}.out; \
		result=$${input%.job}.result; \
		diff_file=$${input%.job}.diff; \
		if diff -q $$result $$output > /dev/null; then \
			passed_tests=$$((passed_tests + 1)); \
		else \
			test_name=$$(basename $$input .job); \
			echo "Failed $$test_name: See file $$diff_file"; \
			diff $$result $$output | tee $$diff_file; \
		fi; \
		for backup in $(wildcard $${input%.in}-*.bck); do \
			backup_result=$${backup%.bck}.backupresult; \
			backup_diff_file=$${backup%.bck}.backupdiff; \
			if diff -q $$backup_result $$backup > /dev/null; then \
				: \
			else \
				test_name=$$(basename $$backup .bck); \
				echo "Failed $$test_name: See file $$backup_diff_file"; \
				diff $$backup_result $$backup | tee $$backup_diff_file; \
			fi; \
		done; \
	done; \
	echo ""; \
	echo "Tests passed $$passed_tests out of $$total_tests"; \
	$(MAKE) -s clean

test_public: kvs
	@cp ./kvs ../tests-public/kvs
	@../tests-public/run_ex1.sh kvs
	@../tests-public/run_ex2.sh kvs
	@$(MAKE) -s clean