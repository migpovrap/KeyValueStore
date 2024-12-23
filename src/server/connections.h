#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>

struct ClientFIFOs {
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path; 
};

typedef struct ClientThreads {
  pthread_t thread;
  struct ClientThreads* next;
} ClientThreads;


void* connection_listener(void* args);

#endif