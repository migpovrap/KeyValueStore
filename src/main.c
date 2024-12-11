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

int main(int argc, char *argv[]) {

  if (kvs_init(STDERR_FILENO)) {
    char error_message[256];
    snprintf(error_message, sizeof(error_message), "Failed to initialize KVS\n");
    write(STDERR_FILENO, error_message, sizeof(error_message));
    return 1;
  }

  if (argc != 4 || TYPECHECK_ARG(argv[2]) || TYPECHECK_ARG(argv[3])) {
    fprintf(stderr, "Usage: %s <directory_path> <number_concurrent_backups>\
    <max_number_threads>\n", argv[0]);
    return 1;
  }
  max_concurrent_backups = atoi(argv[2]);
  int max_threads = atoi(argv[3]);

  JobQueue *queue = create_job_queue(argv[1]);

  pthread_t threads[max_threads];
  backup_forks_pids = (pid_t *) malloc(
  sizeof(pid_t) * (size_t) max_concurrent_backups);

  for (int i = 0; i < max_threads && i < queue->num_files; ++i)
    pthread_create(&threads[i], NULL, process_file, (void *)queue);

  for (int i = 0; i < max_threads && queue->num_files; ++i)
    pthread_join(threads[i], NULL);

  for (int i = 0; i < max_concurrent_backups; i++) {
    kill(backup_forks_pids[i], SIGTERM);
    waitpid(backup_forks_pids[i], NULL, 0);
  }

  free(backup_forks_pids);
  destroy_jobs_queue(queue);
  free(queue);
  kvs_terminate(STDERR_FILENO);
  return 0;
}
