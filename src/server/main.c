#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <stdatomic.h>
#include <semaphore.h>

#include "constants.h"
#include "common/protocol.h"
#include "parser.h"
#include "operations.h"
#include "io.h"
#include "notifications.h"
#include "connections.h"
#include "jobs_manager.h"
#include "pthread.h"



pthread_mutex_t n_current_backups_lock = PTHREAD_MUTEX_INITIALIZER;

ClientSubscriptions all_subscriptions = {PTHREAD_MUTEX_INITIALIZER, NULL};

size_t active_backups = 0;     // Number of active backups
size_t max_backups;            // Maximum allowed simultaneous backups
size_t max_threads;            // Maximum allowed simultaneous threads
char* jobs_directory = NULL;   // Directory containing the jobs files

atomic_bool connection_listener_alive = 1;

sem_t max_clients; // Maximun allowed simultaneous connected clients

// Signal handler functions
_Atomic int sigusr1_received = 0;
_Atomic int sigint_received = 0;

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
      join_all_client_threads();
      atomic_store(&sigusr1_received, 0);
    }
    sleep(1); // Small sleep between checks to avoid busy-waiting
  }
  return NULL;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    write_str(STDERR_FILENO, "Usage: ");
    write_str(STDERR_FILENO, argv[0]);
    write_str(STDERR_FILENO, " <jobs_dir>");
		write_str(STDERR_FILENO, " <max_threads>");
		write_str(STDERR_FILENO, " <max_backups>");
    write_str(STDERR_FILENO, " <server_fifo_path> \n");
    return 1;
  }

  jobs_directory = argv[1];
  char* server_fifo_path = argv[4];   // Server fifo name

  char* endptr;
  max_backups = strtoul(argv[3], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_proc value\n");
    return 1;
  }

  max_threads = strtoul(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_threads value\n");
    return 1;
  }

	if (max_backups <= 0) {
		write_str(STDERR_FILENO, "Invalid number of backups\n");
		return 0;
	}

	if (max_threads <= 0) {
		write_str(STDERR_FILENO, "Invalid number of threads\n");
		return 0;
	}

  if (kvs_init()) {
    write_str(STDERR_FILENO, "Failed to initialize KVS\n");
    return 1;
  }

  DIR* dir = opendir(argv[1]);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory: %s\n", argv[1]);
    return 0;
  }
  
  // Initialize the semaphore with the maximum number of clients the server can support
  sem_init(&max_clients, 0, MAX_SESSION_COUNT);

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
  pthread_t sigusr1_manager;
  pthread_create(&sigusr1_manager, NULL, sigusr1_handler_manager, NULL);

  // Create a thread to listen on the server fifo non blocking.
  pthread_t server_listener;
  if (pthread_create(&server_listener, NULL, connection_listener, server_fifo_path) != 0) {
    fprintf(stderr, "Failed to create connection manager thread\n");
    return 1;
  }

  dispatch_threads(dir);

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory\n");
    return 0;
  }

  while (active_backups > 0) {
    wait(NULL);
    active_backups--;
  }

  // Check to see when SIGINT is sent by the terminal (CTRL-C)
  while (!atomic_load(&sigint_received)) {
    sleep(1);
  }
  
  // Signal the connection manager thread to exit
  atomic_store(&connection_listener_alive, 0);
  pthread_join(server_listener, NULL);

  kvs_terminate();

  pthread_cancel(sigusr1_manager);
  pthread_join(sigusr1_manager, NULL);

  sem_destroy(&max_clients);

  return 0;
}
