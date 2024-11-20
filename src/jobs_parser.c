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

void parse_line(const char* line) {

  // Modifiable copy of the line string
  char line_copy[PIPE_BUF];
  strncpy(line_copy, line, PIPE_BUF);
  line_copy[PIPE_BUF - 1] = '\0'; // Makes sure that the new string is null-terminated

  char *command = strtok(line_copy, " ");
  char *command_args = strtok(NULL, " ");

  if (strcmp(command, "WRITE") == 0) {

    printf("%s %s\n", command, command_args);

  }  else if (strcmp(command, "READ") == 0) {

    printf("%s %s\n", command, command_args);

  } else if (strcmp(command, "DELETE") == 0) {

    printf("%s %s\n", command, command_args);

  } else if (strcmp(command, "SHOW") == 0) {

    printf("%s %s\n", command, command_args);

  } else if (strcmp(command, "WAIT") == 0) {

    printf("%s %s\n", command, command_args);

  } else if (strcmp(command, "BACKUP") == 0) {

    printf("%s %s\n", command, command_args);

  } else {
    fprintf(stderr, "Unknow command\n");
  }
}


void read_file(char *file_name_path) {
  int fd = open(file_name_path, O_RDONLY);

  if (fd == -1) {
    perror("Failed to open job file.");
    return;
  }
  FILE *fp = fdopen(fd, "r");
  char *line = NULL;
  size_t len = 0;

  while (getline(&line, &len , fp) != -1) {
    parse_line(line);
  }
  free(line);
  fclose(fp);
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
