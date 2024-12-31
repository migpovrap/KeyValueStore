#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common/constants.h"
#include "common/io.h"

typedef struct ClientData {
  char req_pipe_path[MAX_PIPE_PATH_LENGTH];
  char resp_pipe_path[MAX_PIPE_PATH_LENGTH];
  char notif_pipe_path[MAX_PIPE_PATH_LENGTH];
  int req_fifo_fd;
  int resp_fifo_fd;
  int notif_fifo_fd;
  int client_subs;
  pthread_t notif_thread;
  _Atomic volatile sig_atomic_t terminate;
} ClientData;

/// Initialize client data with the given client id
/// @param client_id The client's unique identifier.
void initialize_client_data(char* client_id);

/// Cancel and join thread, unlink FIFOs and close file descriptors.
void cleanup();

/// Signal handler for SIGINT and SIGTERM.
void signal_handler();

/// Set up signal handling for termination signals.
void setup_signal_handling();

/// Checks for the SIGINT and SIGTERM signals.
void check_terminate_signal();

/// Create the required FIFOs for communication.
int create_fifos();

/// Function assigned to the notification thread.
void* notification_listener();

#endif // CLIENT_UTILS_H
