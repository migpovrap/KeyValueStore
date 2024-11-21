#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <limits.h>
#include "parser.h"
#include "operations.h"

// For some reason it doesnt import this from the
#ifndef DT_REG
#define DT_REG 8
#endif

void read_file(char *job_file_path) {
  int jobfd = open(job_file_path, O_RDONLY);

  if (jobfd == -1) {
    fprintf(stderr, "Failed to open job file.");
    return;
  }

  char jobout_file_path[PATH_MAX];
  // Removes the og file extension I dont like this code Ve Inês
  char *dot = strrchr(job_file_path, '.');
  if (dot && strcmp(dot, ".job") == 0) {
    *dot = '\0';
  }
  snprintf(jobout_file_path, sizeof(jobout_file_path), "%s.out", job_file_path);

  int joboutput = open(jobout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (joboutput == -1) {
    fprintf(stderr, "Failed to create new file.");
    return;
  }

  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  unsigned int delay;
  size_t num_pairs;

  enum Command cmd;
  while ((cmd = get_next(jobfd)) != EOC) {
    switch (cmd) {
      case CMD_WRITE:
        num_pairs = parse_write(jobfd, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_write(num_pairs, keys, values, joboutput)) {
          fprintf(stderr, "Failed to write pair\n");
        }

        break;

      case CMD_READ:
        num_pairs = parse_read_delete(jobfd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_read(num_pairs, keys, joboutput)) {
          fprintf(stderr, "Failed to read pair\n");
        }
        break;

      case CMD_DELETE:
        num_pairs = parse_read_delete(jobfd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_delete(num_pairs, keys, joboutput)) {
          fprintf(stderr, "Failed to delete pair\n");
        }
        break;

      case CMD_SHOW:
        kvs_show(joboutput);
        break;

      case CMD_WAIT:
        if (parse_wait(jobfd, &delay, NULL) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay > 0) {
          fprintf(stderr, "Waiting...\n");
          kvs_wait(delay);
        }
        break;

      case CMD_BACKUP:

        if (kvs_backup()) {
          fprintf(stderr, "Failed to perform backup.\n");
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
      case CMD_EMPTY:
      case EOC:
        break;
    }
 }
  close(jobfd);
  close(joboutput);
}

void list_dir(char *path) {
  DIR *dir = opendir(path);

  if (!dir) {
    perror("Failed to open jobs dir.");
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG && strstr(entry->d_name, ".job") != NULL) {

    char job_file_path[PATH_MAX];
    snprintf(job_file_path, sizeof(job_file_path), "%s/%s", path, entry->d_name);
    
    read_file(job_file_path);
    }
  }
  closedir(dir);
}
