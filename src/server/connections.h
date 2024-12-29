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

void* connection_listener(void* args);
void init_session_buffer();
void* client_request_handler();
void disconnect_all_clients();

#endif