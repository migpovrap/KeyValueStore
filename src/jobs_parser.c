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

void read_file(char *file_name_path) {
  int fd = open(file_name_path, O_RDONLY);

  if (fd == -1) {
    perror("Failed to open job file.");
    return;
  }

  char buffer[PIPE_BUF];
  ssize_t read_bytes;

  while((read_bytes = read(fd, buffer, sizeof(buffer) -1 )) > 0) {
    buffer[read_bytes] = '\0';
    char *line = strtok(buffer, "\n");
    while ((line != NULL)) {
      
      printf("Processing line: %s\n", line);
      line = strtok(NULL, "\n");
    }
  }

  if (read_bytes == -1) {
    perror("Failed to read job file.");
  }

  close(fd);
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
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
    read_file(file_path);
    }
  }
  
  closedir(dir);
}

