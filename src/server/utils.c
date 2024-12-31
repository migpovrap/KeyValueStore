#include "server/utils.h"

#include <signal.h>

extern ServerData* server_data;

void initialize_server_data(char* job_path,
char* max_threads, char* max_backups) {
  char* endptr;
  server_data->max_backups = strtoul(max_backups, &endptr, 10);
  if (*endptr != '\0') {
    write_str(STDERR_FILENO, "Invalid max_backups value.\n");
    cleanup_and_exit(1);
  } else if (*max_backups <= 0) {
		write_str(STDERR_FILENO, "Invalid number of backups.\n");
		cleanup_and_exit(1);
	}
  server_data->max_threads = strtoul(max_threads, &endptr, 10);
  if (*endptr != '\0') {
		write_str(STDERR_FILENO, "Invalid max_threads value.\n");
    cleanup_and_exit(1);
  } else if (*max_threads <= 0) {
		write_str(STDERR_FILENO, "Invalid number of threads.\n");
		cleanup_and_exit(1);
	}

  pthread_mutex_init(&server_data->backups_mutex, NULL);
  pthread_mutex_init(&server_data->all_subscriptions.mutex, NULL);
  server_data->all_subscriptions.subscription_data = NULL;
  server_data->active_backups = 0;
  server_data->jobs_directory = job_path;
  server_data->terminate = 0;
  server_data->sigusr1_received = 0;
}

void handle_sigusr1() {
  server_data->sigusr1_received = 1;
}

void handle_sigint() {
  server_data->terminate = 1;
}

// Thread Function
void* sigusr1_handler_manager() {
  while (!server_data->sigusr1_received) {
    sleep(1); // Small sleep between checks to avoid busy-waiting.
  }
  clear_all_subscriptions();
  disconnect_all_clients();
  server_data->sigusr1_received = 0;
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
  pthread_create(&server_data->sigusr1_listener, NULL,
  sigusr1_handler_manager, NULL);
}

int setup_register_fifo(char* register_fifo_path) {
  if (pthread_create(&server_data->connection_manager, NULL,
  connection_manager, register_fifo_path) != 0) {
    write_str(STDERR_FILENO, "Failed to create connection manager thread.\n");
    return 1;
  }
  return 0;
}

int setup_client_workers() {
  // Allocate memory for the worker threads array.
  server_data->worker_threads = malloc(MAX_SESSION_COUNT * sizeof(pthread_t));
  if (server_data->worker_threads == NULL) {
    write_str(STDERR_FILENO, "Failed to allocate memory for worker threads.\n");
    cleanup_and_exit(1);
  }
  // Create a pool of worker threads.
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_create(&server_data->worker_threads[i], NULL,
    client_request_handler, NULL) != 0) {
      write_str(STDERR_FILENO, "Failed to create worker thread.\n");
      cleanup_and_exit(1);
    }
  }
  return 0;
}

void cleanup_and_exit(int exit_code) {
  // Signal the connection manager thread to exit if it was created.
  if (atomic_load(&server_data->terminate)) {
    atomic_store(&server_data->terminate, 1);
    pthread_join(server_data->connection_manager, NULL);
  }

  // Join worker threads if they were created.
  if (server_data->worker_threads != NULL) {
    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      if (server_data->worker_threads[i] != 0) {
        pthread_cancel(server_data->worker_threads[i]);
        pthread_join(server_data->worker_threads[i], NULL);
      }
    }
    // Free the worker threads array.
    free(server_data->worker_threads);
  }

  kvs_terminate();

  if (server_data->sigusr1_listener != 0) {
    pthread_cancel(server_data->sigusr1_listener);
    pthread_join(server_data->sigusr1_listener, NULL);
  }
  _exit(exit_code);
}
