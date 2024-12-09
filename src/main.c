#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "jobs_parser.h"

int main(int argc, char *argv[]) {

  if (kvs_init(STDERR_FILENO)) {
    fprintf(stderr, "Failed to initialize KVS\n");
    return 1;
  }
  //TODO Ensure atomic operations for all the code
  //TODO Add type checks for the type of arguments (maybe can be done with some macros??)
  if (argc == 4) {
    int max_threads = atoi(argv[3]);
    int max_concurrent_backups = atoi(argv[2]);

    File_list *job_files_list = list_dir(argv[1]);
    Job_data *current_job = job_files_list->job_data;
    pthread_t threads[max_threads];

  int thread_count = 0;

  while (current_job != NULL) {
    for (int i = 0; i < max_threads && current_job != NULL; i++) {
      current_job->max_concurrent_backups = max_concurrent_backups;
      int result = pthread_create(&threads[i], NULL, process_file, (void *)current_job);
      if (result != 0) {
        fprintf(stderr, "Error creating thread: %d\n", result);
        continue;
      }
      thread_count++;
      current_job = current_job->next;
    }

    for (int i = 0; i < thread_count; i++) {
      int result = pthread_join(threads[i], NULL);
      if (result != 0) {
        fprintf(stderr, "Error joining thread: %d\n", result);
      }
    }
    thread_count = 0; // Reset thread count for the next batch
  }
  
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
