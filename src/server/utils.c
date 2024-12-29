#include "server/utils.h"

extern _Atomic int sigusr1_received;
extern _Atomic int sigint_received;
extern pthread_t* worker_threads;
extern size_t max_threads; 
extern pthread_t sigusr1_manager;
extern pthread_t server_listener;
extern atomic_bool connection_listener_alive;

void handle_sigusr1() {
  atomic_store(&sigusr1_received, 1);
}

void handle_sigint() {
  atomic_store(&sigint_received, 1);
}

// SIGUSR1 thread handler function
void* sigusr1_handler_manager() {
  while (1) {
    if (atomic_load(&sigusr1_received)) {
      clear_all_subscriptions();
      disconnect_all_clients();
      atomic_store(&sigusr1_received, 0);
    }
    sleep(1); // Small sleep between checks to avoid busy-waiting
  }
  return NULL;
}

void setup_signals() {
  // Setup signal handler for SIGUSR1
  struct sigaction sa_usr1;
  sa_usr1.sa_handler = handle_sigusr1;
  sigemptyset(&sa_usr1.sa_mask);
  sa_usr1.sa_flags = 0;
  sigaction(SIGUSR1, &sa_usr1, NULL);

  // Setup signal handler for SIGINT
  struct sigaction sa_int;
  sa_int.sa_handler = handle_sigint;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = 0;
  sigaction(SIGINT, &sa_int, NULL);

  // Create a thread to listen for the SIGUSR1 signal
  pthread_create(&sigusr1_manager, NULL, sigusr1_handler_manager, NULL);
}

void setup_client_workers() {
  // Allocate memory for the worker threads array
  worker_threads = malloc(max_threads * sizeof(pthread_t));
  if (worker_threads == NULL) {
    fprintf(stderr, "Failed to allocate memory for worker threads\n");
    return 1;
  }
  // Create a pool of worker threads
  for (size_t i = 0; i < max_threads; i++) {
    if (pthread_create(&worker_threads[i], NULL, client_request_handler, NULL) != 0) {
      fprintf(stderr, "Failed to create worker thread\n");
      return 1;
    }
  }
  return;
}

void setup_server_fifo(char* server_fifo_path) {
  // Create a thread to listen on the server fifo non blocking.
  if (pthread_create(&server_listener, NULL, connection_listener, server_fifo_path) != 0) {
    fprintf(stderr, "Failed to create connection manager thread\n");
    return 1;
  }
  return;
}

void exit_cleanup() {
  // Signal the connection manager thread to exit
  atomic_store(&connection_listener_alive, 0);
  pthread_join(server_listener, NULL);

  // Join worker threads
  for (size_t i = 0; i < max_threads; i++) {
    pthread_cancel(worker_threads[i]);
    pthread_join(worker_threads[i], NULL);
  }

  // Free the worker threads array
  free(worker_threads);

  kvs_terminate();

  pthread_cancel(sigusr1_manager);
  pthread_join(sigusr1_manager, NULL);
}