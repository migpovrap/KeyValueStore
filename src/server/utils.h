#ifndef UTILS_H
#define UTILS_H

#include <signal.h>
#include <stddef.h>
#include <stdio.h>

#include "connections.h"

// Signal handler for SIGUSR1
void handle_sigusr1();

// Signal handler for SIGINT
void handle_sigint();

// Thread function to manage SIGUSR1 signal handling
void* sigusr1_handler_manager();

// Setup signal handlers for SIGUSR1 and SIGINT
void setup_signals();

// Setup client worker threads
void setup_client_workers();

// Setup server FIFO listener thread
void setup_server_fifo(char* server_fifo_path);

// Cleanup resources on exit
void exit_cleanup();

#endif // UTILS_H