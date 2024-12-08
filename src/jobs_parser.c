#include "parser.h"
#include "operations.h"
#include "jobs_parser.h"

//TODO Change all the functions to use the data struct and only pass pointers for it

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

void cmd_backup(int max_backups, int *backup_counter, char *job_file_path) {
  char backupout_file_path[PATH_MAX];
  snprintf(backupout_file_path, sizeof(backupout_file_path), "%s-%d.bck", job_file_path, *backup_counter);
  fprintf(stderr, "Creating backup file: %s\n", backupout_file_path);
  // Creates the file where the backup will be written
  int backupoutputfd = open(backupout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (backupoutputfd == -1) {
    fprintf(stderr, "Failed to create new backup file.\n");
    return;
  }
  kvs_backup(max_backups, backupoutputfd); //FIXME Function as apparent memory leaks and needs mutex (program gets stuck here)
  close(backupoutputfd);
  (*backup_counter)++;
}

void read_file(char *job_file_path, int max_concurrent_backups) { //FIXME I dont like passing max_backups here. Inês
  int jobfd = open(job_file_path, O_RDONLY);

  if (jobfd == -1) {
    fprintf(stderr, "Failed to open job file.\n");
    return;
  }

  int backup_counter = 1;

  char jobout_file_path[PATH_MAX];

  job_file_path[strlen(job_file_path) - 4] = '\0';
  
  // File path with the correct extension for the output file
  snprintf(jobout_file_path, sizeof(jobout_file_path), "%s.out", job_file_path);
  
  // Creates the file where the output will be written
  int joboutputfd = open(jobout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (joboutputfd == -1) {
    fprintf(stderr, "Failed to create new file.");
    return;
  }

  fprintf(stderr, "Reading job: %s\n", job_file_path); //REMOVE
  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};

  enum Command cmd;
  while ((cmd = get_next(jobfd)) != EOC) {
    switch (cmd) {
      case CMD_WRITE:
        fprintf(stderr, "Executing command: CMD_WRITE\n"); //REMOVE
        cmd_write(&jobfd, &keys, &values, &joboutputfd);
        break;

      case CMD_READ:
        fprintf(stderr, "Executing command: CMD_READ\n"); //REMOVE
        cmd_read(&jobfd, &keys, &joboutputfd);
        break;

      case CMD_DELETE:
        fprintf(stderr, "Executing command: CMD_DELETE\n"); //REMOVE
        cmd_delete(&jobfd, &keys, &joboutputfd);
        break;

      case CMD_SHOW:
        fprintf(stderr, "Executing command: CMD_SHOW\n"); //REMOVE
        kvs_show(joboutputfd);
        break;

      case CMD_WAIT:
        fprintf(stderr, "Executing command: CMD_WAIT\n"); //REMOVE
        cmd_wait(&jobfd, &joboutputfd);
        break;

      case CMD_BACKUP:
        fprintf(stderr, "Executing command: CMD_BACKUP\n"); //REMOVE
        cmd_backup(max_concurrent_backups, &backup_counter, job_file_path);
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n"); //REMOVE
        break;

      case CMD_HELP:
      case CMD_EMPTY:
      case EOC:
        break;
    }
  }
  close(jobfd);
  close(joboutputfd);
}

void *process_file(void *arg) {
  Job_data *job_data = (Job_data *)arg;
  read_file(job_data->job_file_path, job_data->max_concurrent_backups);
  return NULL;
}

File_list *list_dir(char *path) {
  DIR *dir = opendir(path);
  if (!dir) {
    perror("Failed to open jobs dir.");
    return NULL;
  }

  struct dirent *current_file;
  File_list *job_files_list = malloc(sizeof(File_list));
  job_files_list->num_files = 0;
  job_files_list->job_data = NULL;
  
  while ((current_file = readdir(dir)) != NULL) {
    if (current_file->d_type == DT_REG && strstr(current_file->d_name, ".job") != NULL) {
      Job_data *job_data = malloc(sizeof(Job_data));
      size_t path_len = strlen(path) + strlen(current_file->d_name) + 2; // +2 for '/' and null terminator
      job_data->job_file_path = malloc(path_len * sizeof(char));
      snprintf(job_data->job_file_path, path_len, "%s/%s", path, current_file->d_name);
      add_job_data(&job_files_list, job_data);
    }
  }
  closedir(dir);
  return job_files_list;
}

void clear_job_data_list(File_list** job_files_list) {
  if ((*job_files_list)->job_data == NULL)
    return;
  Job_data* current_job = (*job_files_list)->job_data;
  while (current_job != NULL) {
    Job_data* next_job = current_job->next;
    free(current_job->job_file_path);
    free(current_job);
    current_job = next_job;
  }
  (*job_files_list)->job_data = NULL; //After it clears the linked list the head is set to null
}

void add_job_data(File_list** job_files_list, Job_data* new_job_data) {
  if ((*job_files_list)->job_data == NULL) {
      (*job_files_list)->job_data = new_job_data;
  } else {
    Job_data* current_job = (*job_files_list)->job_data;
    while (current_job->next != NULL) {
      current_job = current_job->next;
    }
    current_job->next = new_job_data;
    new_job_data->next = NULL;
    (*job_files_list)->num_files++;
  }
}