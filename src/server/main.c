#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "jobs_manager.h"
#include "server/io.h"
#include "server/utils.h"

ServerData* server_data;

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

  server_data = malloc(sizeof(ServerData));
  if (!server_data) {
    fprintf(stderr, "Failed to allocate memory for server data.\n");
    cleanup_and_exit(1);
  }

  initialize_server_data(argv[1], argv[2], argv[3]);

  if (kvs_init()) {
    write_str(STDERR_FILENO, "Failed to initialize KVS.\n");
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

  setup_registration_fifo(argv[4]);
  
  dispatch_threads(dir);

  if (closedir(dir) == -1) {
    write_str(STDERR_FILENO, "Failed to close directory.\n");
    cleanup_and_exit(1);
  }

  while (server_data->active_backups > 0) {
    wait(NULL);
    server_data->active_backups--;
  }

  while (!atomic_load(&server_data->terminate))
    sleep(1);
  
  cleanup_and_exit(0);
}
