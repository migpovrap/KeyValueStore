#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <pthread.h>
#include "common/constants.h"
#include <semaphore.h>

typedef struct Buffer {
  struct ClientFIFOs* consumer_producer_buffer[MAX_SESSION_COUNT];
  int input;
  int output;
  sem_t full;
  sem_t empty;
  pthread_mutex_t buffer_mutex;
} Buffer;

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