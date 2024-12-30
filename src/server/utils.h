#ifndef UTILS_H
#define UTILS_H

#include <signal.h>
#include <stddef.h>
#include <stdio.h>

#include "connections.h"

/// Signal handler for SIGUSR1.
void handle_sigusr1();

/// Signal handler for SIGINT.
void handle_sigint();

/// Thread function to manage SIGUSR1 signal handling.
/// @return void* A pointer to the thread's return value.
void* sigusr1_handler_manager();

/// Setup signal handlers for SIGUSR1 and SIGINT.
void setup_signal_handling();

/// Setup server FIFO listener thread.
/// @param server_fifo_path The path to the server FIFO.
/// @return int Returns 0 on success, or a negative error code on failure.
int setup_server_fifo(char* server_fifo_path);

/// Setup client worker threads.
/// @return int Returns 0 on success, or a negative error code on failure.
int setup_client_workers();

/// Cleanup resources and exit the program.
/// @param exit_code The exit code to be returned by the program.
void cleanup_and_exit(int exit_code);

#endif // UTILS_H
