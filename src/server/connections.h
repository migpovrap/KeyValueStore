#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <common/constants.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct ClientListenerData {
  pthread_t thread;
  atomic_bool terminate;
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path;
  struct ClientListenerData* next; 
} ClientListenerData;

typedef struct {
  ClientListenerData session_data[MAX_SESSION_COUNT];
  int in;
  int out;
  sem_t full;
  sem_t empty;
  pthread_mutex_t buffer_lock;
} RequestBuffer;

/// Listens for incoming connections.
/// @param args Arguments passed to the listener (if any).
/// @return A pointer to the result (if any).
void* connection_listener(void* args);

/// Initializes the session buffer.
void initialize_session_buffer();

/// Handles client requests.
void* client_request_handler();

/// Disconnects all clients.
void disconnect_all_clients();

#endif