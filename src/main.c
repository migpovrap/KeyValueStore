#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "jobs_parser.h"

int main(int argc, char *argv[]) {

  if (kvs_init(STDERR_FILENO)) {
    fprintf(stderr, "Failed to initialize KVS\n");
    return 1;
  }

  //TODO Add type checks for the type of arguments (maybe can be done with some macros??)
  if (argc == 4) {
    int max_threads = atoi(argv[3]);
    fprintf(stderr, "%d\n", max_threads); //FIXME Temp uses only for runnig test before multithread implementation
    int max_concurrent_backups = atoi(argv[2]);

    File_list *job_files_path = list_dir(argv[1]);

    for (int i = 0; i < job_files_path->num_files; i++) {
      read_file(job_files_path->path_job_files[i], max_concurrent_backups);
      free(job_files_path->path_job_files[i]);
    }
    
    free(job_files_path->path_job_files);
    free(job_files_path);
    kvs_terminate(STDERR_FILENO);
    return 0;
  }

  if (argc < 2 || argc >= 4) {
    fprintf(stderr,
      "Not enough arguments to start IST-KVS\n"
      "The correct format is:\n"
      "./%s <jobdir_file> <MAX_BACKUPS> <MAX_THREADS>\n", argv[0]);
    return 0;
  }

  fprintf(stderr,
    "No arguments provided, cannot start IST-KVS\n"
    "The correct format is:\n"
    "./%s <jobdir_file> <MAX_BACKUPS> <MAX_THREADS>\n", argv[0]);
  return 0;
}
