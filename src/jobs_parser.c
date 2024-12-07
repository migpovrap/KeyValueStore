#include "parser.h"
#include "operations.h"
#include "jobs_parser.h"


void cmd_write(int *jobfd, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], char (*values)[MAX_WRITE_SIZE][MAX_STRING_SIZE], int *joboutput) {
  size_t num_pairs;
  num_pairs = parse_write(*jobfd, *keys, *values, MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (kvs_write(num_pairs, *keys, *values, *joboutput)) {
    fprintf(stderr, "Failed to write pair\n");
  }
}

void cmd_read(int *jobfd, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], int *joboutput) {
  size_t num_pairs;
  num_pairs = parse_read_delete(*jobfd, *keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (kvs_read(num_pairs, *keys, *joboutput)) {
    fprintf(stderr, "Failed to read pair\n");
  }
}

void cmd_delete(int *jobfd, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], int *joboutput) {
  size_t num_pairs;
  num_pairs = parse_read_delete(*jobfd, *keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (kvs_delete(num_pairs, *keys, *joboutput)) {
    fprintf(stderr, "Failed to delete pair\n");
  }
}

void cmd_wait(int *jobfd, int *joboutput) {
  unsigned int delay;
  if (parse_wait(*jobfd, &delay, NULL) == -1) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (delay > 0) {
    kvs_wait(delay, *joboutput);
  }
}

void cmd_backup(int max_backups, int *backupoutput, int *joboutput) {
  if (kvs_backup(max_backups, *backupoutput, *joboutput)) {
    fprintf(stderr, "Failed to perform backup.\n");
  }
}

void read_file(char *job_file_path, int max_backups) { //FIXME I dont like passing in this function these value doesnt feel right ve Inês
  int jobfd = open(job_file_path, O_RDONLY);

  if (jobfd == -1) {
    fprintf(stderr, "Failed to open job file.");
    return;
  }

  char jobout_file_path[PATH_MAX];
  char backupout_file_path[PATH_MAX];

  //FIXME Removes the og file extension I dont like this code ve Inês
  char *dot = strrchr(job_file_path, '.');
  if (dot && strcmp(dot, ".job") == 0) {
    *dot = '\0';
  }
  
  // A file path with the correct extension for each file needed
  snprintf(jobout_file_path, sizeof(jobout_file_path), "%s.out", job_file_path);
  snprintf(backupout_file_path, sizeof(jobout_file_path), "%s.bck", job_file_path);

  // Creates the file where the output will be written
  int joboutputfd = open(jobout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (joboutputfd == -1) {
    fprintf(stderr, "Failed to create new file.");
    return;
  }

  // Creates the file where the backup will be written
  int backupoutputfd = open(backupout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (backupoutputfd == -1) {
    fprintf(stderr, "Failed to create new backup file.");
    return;
  }


  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};

  enum Command cmd;
  while ((cmd = get_next(jobfd)) != EOC) {
    switch (cmd) {
      case CMD_WRITE:
        cmd_write(&jobfd, &keys, &values, &joboutputfd);
        break;

      case CMD_READ:
        cmd_read(&jobfd, &keys, &joboutputfd);
        break;

      case CMD_DELETE:
        cmd_delete(&jobfd, &keys, &joboutputfd);
        break;

      case CMD_SHOW:
        kvs_show(joboutputfd);
        break;

      case CMD_WAIT:
        cmd_wait(&jobfd, &joboutputfd);
        break;

      case CMD_BACKUP:
        cmd_backup(max_backups, &backupoutputfd, &joboutputfd);
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
  close(joboutputfd);
  close(backupoutputfd);
}

void list_dir(char *path, int max_backups) { //FIXME I dont like passing in this function these value doesnt feel right ve Inês
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
    
    read_file(job_file_path, max_backups); //FIXME I dont like passing in this function these value doesnt feel right ve Inês
    }
  }
  closedir(dir);
}
