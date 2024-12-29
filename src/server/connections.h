#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>
#include <stdatomic.h>
#include <common/constants.h>

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

/**
 * @brief Listens for incoming connections.
 *
 * This function runs in a separate thread and listens for incoming client connections.
 * It accepts connections and delegates them to the appropriate handler.
 *
 * @param args Arguments passed to the listener (if any).
 * @return A pointer to the result (if any).
 */
void* connection_listener(void* args);

/**
 * @brief Initializes the session buffer.
 *
 * This function sets up the necessary buffer for handling client sessions.
 * It should be called before any client sessions are processed.
 */
void initialize_session_buffer();

/**
 * @brief Handles client requests.
 *
 * This function processes incoming client requests. It runs in a separate thread
 * for each client and handles the communication and request processing.
 *
 * @return A pointer to the result (if any).
 */
void* client_request_handler();

/**
 * @brief Disconnects all clients.
 *
 * This function forcefully disconnects all connected clients. It is typically used
 * during server shutdown or when a critical error occurs.
 */
void disconnect_all_clients();

#endif