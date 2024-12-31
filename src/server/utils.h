#ifndef UTILS_H
#define UTILS_H

#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>

#include "connections.h"
#include "notifications.h"
#include "operations.h"
#include "server/io.h"

typedef struct ServerData {
  char* jobs_directory;                             // Directory containing the jobs files
  size_t max_threads;                               // Maximum allowed simultaneous threads.
  size_t max_backups;                               // Maximum allowed simultaneous backups.
  size_t active_backups;                            // Number of active backups.
  pthread_mutex_t backups_mutex;                    // Mutex to lock number of active backups.
  pthread_t connection_manager;                     // Thread to listen for client connections to the server.
  pthread_t* worker_threads;                        // Array of client worker threads.
  sig_atomic_t sigusr1_received;                    // Flag indicating SIGUSR1 signal was recieved.
  _Atomic volatile sig_atomic_t terminate;          // Flag indicating SIGINT or SIGTERM signal was recieved.
  ClientSubscriptions all_subscriptions;            // Linked list holding subscription data for all clients.
} ServerData;

/// Initializes the server data with the given parameters.
/// @param job_path The path to the job configuration file.
/// @param max_threads The maximum number of threads the server can use to process job files.
/// @param max_backups The maximum number of concurrent backups the server can maintain.
void initialize_server_data(char* job_path, char* max_threads, char* max_backups);

/// Signal handler for SIGUSR1.
void handle_sigusr1();

/// Signal handler for SIGINT.
void handle_sigint();

/// Setup signal handlers for SIGUSR1 and SIGINT.
void setup_signal_handling();

/// Setup server FIFO listener thread.
/// @param register_fifo_path The path to the server register FIFO.
/// @return int Returns 0 on success, or a negative error code on failure.
int setup_register_fifo(char* register_fifo_path);

/// Setup client worker threads.
/// @return int Returns 0 on success, or a negative error code on failure.
int setup_client_workers();

/// Cleanup resources and exit the program.
/// @param exit_code The exit code to be returned by the program.
void cleanup_and_exit(int exit_code);

#endif // UTILS_H
