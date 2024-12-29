#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>
#include <stdatomic.h>

typedef struct ClientListenerData {
  pthread_t thread;
  atomic_bool client_listener_alive;
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path;
  struct ClientListenerData* next; 
} ClientListenerData;

void* connection_listener(void* args);

#endif