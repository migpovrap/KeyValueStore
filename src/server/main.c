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
#include <pthread.h>

#include "constants.h"
#include "common/protocol.h"
#include "parser.h"
#include "operations.h"
#include "io.h"
#include "notifications.h"
#include "connections.h"
#include "jobs_manager.h"
#include "server/utils.c"


pthread_mutex_t n_current_backups_lock = PTHREAD_MUTEX_INITIALIZER;

ClientSubscriptions all_subscriptions = {PTHREAD_MUTEX_INITIALIZER, NULL};

size_t active_backups = 0;     // Number of active backups
size_t max_backups;            // Maximum allowed simultaneous backups
size_t max_threads;            // Maximum allowed simultaneous threads
char* jobs_directory = NULL;   // Directory containing the jobs files
pthread_t* worker_threads;     // Array of client worker threads
pthread_t sigusr1_manager;     // Thread to listen for sigusr1 in a signal safe manner
pthread_t server_listener;     // Thread to listen for server connections

atomic_bool connection_listener_alive = 1;

_Atomic int sigusr1_received = 0;
_Atomic int sigint_received = 0;

int main(int argc, char** argv) {
  if (argc < 4) {
    write_str(STDERR_FILENO, "Usage: ");
    write_str(STDERR_FILENO, argv[0]);
    write_str(STDERR_FILENO, " <jobs_dir>");
		write_str(STDERR_FILENO, " <max_threads>");
		write_str(STDERR_FILENO, " <max_backups>");
    write_str(STDERR_FILENO, " <server_fifo_path> \n");
    cleanup_and_exit(1);
  }

  jobs_directory = argv[1];

  char* endptr;
  max_backups = strtoul(argv[3], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_proc value\n");
    cleanup_and_exit(1);
  }

  max_threads = strtoul(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_threads value\n");
    cleanup_and_exit(1);
  }

	if (max_backups <= 0) {
		write_str(STDERR_FILENO, "Invalid number of backups\n");
		cleanup_and_exit(1);
	}

	if (max_threads <= 0) {
		write_str(STDERR_FILENO, "Invalid number of threads\n");
		cleanup_and_exit(1);
	}

  if (kvs_init()) {
    write_str(STDERR_FILENO, "Failed to initialize KVS\n");
    cleanup_and_exit(1);
  }

  DIR* dir = opendir(argv[1]);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory: %s\n", argv[1]);
    cleanup_and_exit(1);
  }
  
  initialize_session_buffer();

  setup_client_workers();

  setup_signal_handling();

  setup_server_fifo(argv[4]);
  
  dispatch_threads(dir);

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory\n");
    cleanup_and_exit(1);
  }

  while (active_backups > 0) {
    wait(NULL);
    active_backups--;
  }

  // Check to see when SIGINT is sent by the terminal (CTRL-C)
  while (!atomic_load(&sigint_received)) {
    sleep(1);
  }
  
  cleanup_and_exit(0);
}
