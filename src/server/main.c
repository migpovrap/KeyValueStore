#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <stdatomic.h>

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

int main(int argc, char** argv) {
  if (argc < 4) {
    write_str(STDERR_FILENO, "Usage: ");
    write_str(STDERR_FILENO, argv[0]);
    write_str(STDERR_FILENO, " <jobs_dir>");
		write_str(STDERR_FILENO, " <max_threads>");
		write_str(STDERR_FILENO, " <max_backups>");
    write_str(STDERR_FILENO, " <registry_fifo_path> \n");
    return 1;
  }

  jobs_directory = argv[1];
  char* registry_fifo_path = argv[4];   // Registry fifo name

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
  
  // Create a thread to listen on the registry fifo non blocking.
  pthread_t registry_listener;
  if (pthread_create(&registry_listener, NULL, connection_listener, registry_fifo_path) != 0) {
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


  // Sleeps for 20 sec, I don't know how the server should end, when it finishes the jobs or does it have a exit command??
  sleep(20);

  // Signal the connection manager thread to exit
  atomic_store(&connection_listener_alive, 0);
  pthread_join(registry_listener, NULL);

  kvs_terminate();

  return 0;
}
