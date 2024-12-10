#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jobs_parser.h"
#include "operations.h"

int max_concurrent_backups;
pid_t *backup_forks_pids;

int main(int argc, char *argv[]) {

  if (kvs_init(STDERR_FILENO)) {
    fprintf(stderr, "Failed to initialize KVS\n");
    return 1;
  }
  //TODO Ensure atomic operations for all the code
  //TODO Add type checks for the type of arguments (maybe can be done with some macros??)
  if (argc == 4) {
    int max_threads = atoi(argv[3]);
    max_concurrent_backups = atoi(argv[2]);

    File_list *job_files_list = list_dir(argv[1]);
    Job_data *current_job = job_files_list->job_data;
    pthread_t threads[max_threads];
    backup_forks_pids = (pid_t *)malloc(sizeof(pid_t) * (size_t)max_concurrent_backups);

    for (int i = 0; i < max_threads && current_job != NULL; ++i) {
      pthread_create(&threads[i], NULL, process_file, (void *)current_job);
      current_job = current_job->next;
    }

    for (int i = 0; i < max_threads; i++) {
      pthread_join(threads[i], NULL);
    }

    // Wait for all child processes to terminate
    for (int i = 0; i < max_concurrent_backups; i++) {
      printf("Sending SIGTERM to fork: %d\n", backup_forks_pids[i]); // REMOVE
      kill(backup_forks_pids[i], SIGTERM);
      printf("Waiting for fork: %d\n", backup_forks_pids[i]); // REMOVE
      waitpid(backup_forks_pids[i], NULL, 0);
    }

    free(backup_forks_pids);
    clear_job_data_list(&job_files_list);
    free(job_files_list);
    kvs_terminate(STDERR_FILENO);
    return 0;
  }

  if (argc < 2 || argc >= 4) {
    fprintf(stderr,
      "Not enough arguments to start IST-KVS\n"
      "The correct format is:\n"
      "./%s <jobdir_file> <MAX_BACKUPS> <MAX_THREADS>\n", argv[0]);
    kvs_terminate(STDERR_FILENO);
    return 1;
  }

  fprintf(stderr,
    "No arguments provided, cannot start IST-KVS\n"
    "The correct format is:\n"
    "./%s <jobdir_file> <MAX_BACKUPS> <MAX_THREADS>\n", argv[0]);
  kvs_terminate(STDERR_FILENO);
  return 1;
}
