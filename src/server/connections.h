#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>
#include "common/constants.h"
#include <semaphore.h>
#include <stdatomic.h>

typedef struct ClientThreads {
  pthread_t thread;
  atomic_bool client_listener_alive;
  struct ClientThreads* next;
  pthread_mutex_t thread_data_lock;
} ClientThreads;

struct ClientFIFOs {
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path; 
  ClientThreads* thread_data;
};

void* connection_listener(void* args);

#endif