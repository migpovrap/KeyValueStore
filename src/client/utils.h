#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>

#include "common/constants.h"

typedef struct ClientData {
  char req_pipe_path[MAX_PIPE_PATH_LENGTH];
  char resp_pipe_path[MAX_PIPE_PATH_LENGTH];
  char notif_pipe_path[MAX_PIPE_PATH_LENGTH];
  int req_fifo_fd;
  int resp_fifo_fd;
  int notif_fifo_fd;
  pthread_t notif_thread;
  int client_subs;
    volatile sig_atomic_t terminate;
} ClientData;

/// Initialize client data with the given client id
/// @param client_id The client's unique identifier.
void initialize_client_data(char* client_id);

/// Unlink FIFOs and close file descriptors
void cleanup_fifos();

/// Signal handler for SIGINT and SIGTERM
void signal_handler();

/// Set up signal handling for termination signals
void setup_signal_handling();

/// Create the required FIFOs for communication
int create_fifos();

/// Function assigned to the notification thread
void* notification_listener();

/// Clean up and exit the program with the given exit code
void cleanup_and_exit(int exit_code);

/// Checks for the SIGINT and SIGTERM signals
void check_terminate_signal();
#endif // UTILS_H
