#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "common/constants.h"

typedef struct ClientData {
  volatile atomic_bool terminate;
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path;
} ClientData;

typedef struct {
  ClientData** session_data;
  int in;
  int out;
  sem_t full;
  sem_t empty;
  pthread_mutex_t buffer_mutex;
} RequestBuffer;

/// Listens for incoming connections.
/// @param args Arguments passed to the listener (if any).
/// @return A pointer to the result (if any).
void* connection_manager(void* args);

/// Initializes the session buffer.
void initialize_session_buffer();

/// Cleanup the session buffer.
void cleanup_session_buffer();

/// Handles client requests.
void* client_request_handler();

/// Disconnects all clients.
void disconnect_all_clients();

#endif
