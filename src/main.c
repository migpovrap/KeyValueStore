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
    int max_backups = atoi(argv[2]);
    list_dir(argv[1], max_backups); //FIXME I dont like this way maybe some auxialiry functions or move some logic from job_parser to main or other files??
    kvs_terminate(STDERR_FILENO);
    return 0;
  }

  if (argc < 3 && argc > 1) {
    fprintf(stderr,
      "Not enough arguments to start IST-KVS\n"
      "The correct format is:\n"
      "./%s <jobdir_file> <MAX_BACKUPS>\n", argv[0]);
    return 0;
  }

  fprintf(stderr,
    "No arguments provided, cannot start IST-KVS\n"
    "The correct format is:\n"
    "./%s <jobdir_file> <MAX_BACKUPS>\n", argv[0]);
  return 0;
}
