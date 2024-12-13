#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jobs_parser.h"
#include "operations.h"

#define TYPECHECK_ARG(arg) ({ \
  char *endptr; \
  strtol((arg), &endptr, 10); \
  *endptr != '\0'; \
})

int max_concurrent_backups;
pid_t *backup_forks_pids;
int concurrent_backups = 0;
pthread_mutex_t backup_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {

  if (kvs_init()) {
    fprintf(stderr, "Failed to initialize KVS\n");
    return 1;
  }

  if (argc != 4 || TYPECHECK_ARG(argv[2]) || TYPECHECK_ARG(argv[3])) {
    fprintf(stderr, "Usage: %s <directory_path> <number_concurrent_backups>\
    <max_number_threads>\n", argv[0]);
    return 1;
  }

  JobQueue *queue = create_job_queue(argv[1]);
  max_concurrent_backups = atoi(argv[2]);
  int max_threads = atoi(argv[3]);
  int max_files = queue->num_files;
  pthread_t threads[max_threads];

  backup_forks_pids = (pid_t *) malloc(
  sizeof(pid_t) * (size_t) max_concurrent_backups);

  for (int i = 0; i < max_concurrent_backups; ++i)
    backup_forks_pids[i] = -1; // Initialize with an invalid PID

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_create(&threads[i], NULL, process_file, (void *)queue);

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_join(threads[i], NULL);

  for (int i = 0; i < max_concurrent_backups; i++)
    if (backup_forks_pids[i] >= 0) {
      kill(backup_forks_pids[i], SIGTERM);
      waitpid(backup_forks_pids[i], NULL, 0);
    }

  free(backup_forks_pids);
  destroy_jobs_queue(queue);
  pthread_mutex_destroy(&backup_mutex);
  kvs_terminate();
  return 0;
}
