#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>

typedef struct ClientThreads {
  pthread_t thread;
  _Atomic int client_listener_alive;
  struct ClientThreads* next;
} ClientThreads;

struct ClientFIFOs {
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path; 
  ClientThreads* thread_data;
};

void* connection_listener(void* args);

#endif