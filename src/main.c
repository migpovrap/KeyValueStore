#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jobs_parser.h"
#include "operations.h"
#include "macros.h"

sem_t backup_semaphore;
_Atomic int child_terminated = 0;

int main(int argc, char *argv[]) {
  CHECK_RETURN_ONE(kvs_init(), "Failed to initialize KVS.");

  if (argc != 4 || TYPECHECK_ARG(argv[2]) || TYPECHECK_ARG(argv[3])) {
    fprintf(stderr, "Usage: %s <directory_path> <number_concurrent_backups>\
    <max_number_threads>\n", argv[0]);
    return 1;
  }

  // Sigaction struct to handle SIGCHLD signals
  struct sigaction signal_action;
  signal_action.sa_handler = semaphore_aux;
  sigemptyset(&signal_action.sa_mask); // Initialize the signal set to empty
  signal_action.sa_flags = SA_RESTART; // Restart interrupted system calls

  CHECK_RETURN_ONE(sigaction(SIGCHLD, &signal_action, NULL), "sigaction");
  
  JobQueue *queue = create_job_queue(argv[1]);
  CHECK_NULL(queue, "Failed to create job queue.");
  sem_init(&backup_semaphore, 0, (unsigned int)atoi(argv[2])); // 0 means semaphore is shared between threads of the same process
  int max_threads = atoi(argv[3]);
  int max_files = queue->num_files;
  pthread_t threads[max_threads];

  pthread_t semaphore_thread;
  pthread_create(&semaphore_thread, NULL, semaphore_aux_thread, NULL);

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_create(&threads[i], NULL, process_file, (void *)queue);

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_join(threads[i], NULL);
  
  pthread_join(semaphore_thread, NULL);

  while (sem_trywait(&backup_semaphore) != 0) // Waits for all child processes to end.
    sleep(1);
  
  sem_destroy(&backup_semaphore);
  destroy_jobs_queue(queue);
  queue = NULL;
  kvs_terminate();
  return 0;
}
