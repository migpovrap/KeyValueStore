#include "server/utils.h"

extern _Atomic int sigusr1_received;
extern _Atomic int sigint_received;
extern pthread_t* worker_threads;
extern pthread_t sigusr1_manager;
extern pthread_t server_listener;
extern atomic_bool terminate;

void handle_sigusr1() {
  atomic_store(&sigusr1_received, 1);
}

void handle_sigint() {
  atomic_store(&sigint_received, 1);
}

// Thread Function
void* sigusr1_handler_manager() {
  while (1) {
    if (atomic_load(&sigusr1_received)) {
      clear_all_subscriptions();
      disconnect_all_clients();
      atomic_store(&sigusr1_received, 0);
    }
    sleep(1); // Small sleep between checks to avoid busy-waiting.
  }
  return NULL;
}

void setup_signal_handling() {
  // SIGUSR1
  struct sigaction sa_usr1;
  sa_usr1.sa_handler = handle_sigusr1;
  sigemptyset(&sa_usr1.sa_mask);
  sa_usr1.sa_flags = 0;
  sigaction(SIGUSR1, &sa_usr1, NULL);

  // SIGINT
  struct sigaction sa_int;
  sa_int.sa_handler = handle_sigint;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = 0;
  sigaction(SIGINT, &sa_int, NULL);

  // Create a thread to listen for the SIGUSR1 signal.
  pthread_create(&sigusr1_manager, NULL, sigusr1_handler_manager, NULL);
}

int setup_server_fifo(char* server_fifo_path) {
  if (pthread_create(&server_listener, NULL, connection_listener, server_fifo_path) != 0) {
    fprintf(stderr, "Failed to create connection manager thread.\n");
    return 1;
  }
  return 0;
}

int setup_client_workers() {
  // Allocate memory for the worker threads array.
  worker_threads = malloc(MAX_SESSION_COUNT * sizeof(pthread_t));
  if (worker_threads == NULL) {
    fprintf(stderr, "Failed to allocate memory for worker threads.\n");
    cleanup_and_exit(1);
  }
  // Create a pool of worker threads.
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_create(&worker_threads[i], NULL, client_request_handler, NULL) != 0) {
      fprintf(stderr, "Failed to create worker thread.\n");
      cleanup_and_exit(1);
    }
  }
  return 0;
}

void cleanup_and_exit(int exit_code) {
  // Signal the connection manager thread to exit if it was created.
  if (atomic_load(&terminate)) {
    atomic_store(&terminate, 1);
    pthread_join(server_listener, NULL);
  }

  // Join worker threads if they were created.
  if (worker_threads != NULL) {
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      if (worker_threads[i] != 0) {
        pthread_cancel(worker_threads[i]);
        pthread_join(worker_threads[i], NULL);
      }
    }
    // Free the worker threads array.
    free(worker_threads);
  }

  kvs_terminate();

  if (sigusr1_manager != 0) {
    pthread_cancel(sigusr1_manager);
    pthread_join(sigusr1_manager, NULL);
  }
  exit(exit_code);
}
