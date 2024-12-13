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

sem_t backup_semaphore;

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
  sem_init(&backup_semaphore, 0, atoi(argv[2])); // 0 means semaphore is shared between threads of the same process
  int max_threads = atoi(argv[3]);
  int max_files = queue->num_files;
  pthread_t threads[max_threads];

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_create(&threads[i], NULL, process_file, (void *)queue);

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_join(threads[i], NULL);

  sem_destroy(&backup_semaphore);
  destroy_jobs_queue(queue);
  queue = NULL;
  kvs_terminate();
  return 0;
}
