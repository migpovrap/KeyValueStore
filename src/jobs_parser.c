#include "jobs_parser.h"


void cmd_write(Job_data* job_data, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], char (*values)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_write(job_data->job_fd, *keys, *values, MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (kvs_write(num_pairs, *keys, *values, job_data->job_output_fd)) {
    fprintf(stderr, "Failed to write pair\n");
  }
}

void cmd_read(Job_data* job_data, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_read_delete(job_data->job_fd, *keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (kvs_read(num_pairs, *keys, job_data->job_output_fd)) {
    fprintf(stderr, "Failed to read pair\n");
  }
}

void cmd_delete(Job_data* job_data, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_read_delete(job_data->job_fd, *keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (kvs_delete(num_pairs, *keys, job_data->job_output_fd)) {
    fprintf(stderr, "Failed to delete pair\n");
  }
}

void cmd_wait(Job_data* job_data) {
  unsigned int delay;
  if (parse_wait(job_data->job_fd, &delay, NULL) == -1) {
    fprintf(stderr, "Invalid command. See HELP for usage\n");
  }

  if (delay > 0) {
    kvs_wait(delay, job_data->job_output_fd);
  }
}

void cmd_backup(Job_data* job_data) {
  char backupout_file_path[PATH_MAX];
  snprintf(backupout_file_path, sizeof(backupout_file_path), "%s-%d.bck", job_data->job_file_path, job_data->backup_counter);
  fprintf(stderr, "Creating backup file: %s\n", backupout_file_path);
  // Creates the file where the backup will be written
  int backupoutputfd = open(backupout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (backupoutputfd == -1) {
    fprintf(stderr, "Failed to create new backup file.\n");
    return;
  }
  kvs_backup(backupoutputfd);
  close(backupoutputfd);
  job_data->backup_counter++;
}

void read_file(Job_data* job_data) {
  job_data->job_fd = open(job_data->job_file_path, O_RDONLY);

  if (job_data->job_fd == -1) {
    fprintf(stderr, "Failed to open job file.\n");
    return;
  }

  char jobout_file_path[PATH_MAX];

  job_data->job_file_path[strlen(job_data->job_file_path) - 3] = '\0';
  
  // File path with the correct extension for the output file
  snprintf(jobout_file_path, sizeof(jobout_file_path), "%sout", job_data->job_file_path);
  
  // Creates the file where the output will be written
  job_data->job_output_fd = open(jobout_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (job_data->job_output_fd == -1) {
    fprintf(stderr, "Failed to create new file.");
    return;
  }

  fprintf(stderr, "Reading job: %s\n", job_data->job_file_path); //REMOVE
  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};

  enum Command cmd;
  while ((cmd = get_next(job_data->job_fd)) != EOC) {
    switch (cmd) {
      case CMD_WRITE:
        fprintf(stderr, "Executing command: CMD_WRITE\n"); //REMOVE
        cmd_write(job_data, &keys, &values);
        break;

      case CMD_READ:
        fprintf(stderr, "Executing command: CMD_READ\n"); //REMOVE
        cmd_read(job_data, &keys);
        break;

      case CMD_DELETE:
        fprintf(stderr, "Executing command: CMD_DELETE\n"); //REMOVE
        cmd_delete(job_data, &keys);
        break;

      case CMD_SHOW:
        fprintf(stderr, "Executing command: CMD_SHOW\n"); //REMOVE
        kvs_show(job_data->job_output_fd);
        break;

      case CMD_WAIT:
        fprintf(stderr, "Executing command: CMD_WAIT\n"); //REMOVE
        cmd_wait(job_data);
        break;

      case CMD_BACKUP:
        fprintf(stderr, "Executing command: CMD_BACKUP\n"); //REMOVE
        cmd_backup(job_data);
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
  close(job_data->job_fd);
  close(job_data->job_output_fd);
}

void *process_file(void *arg) {
  Job_data *job_data = (Job_data *)arg;
  for (; job_data != NULL; job_data = job_data->next) {
    pthread_mutex_lock(&job_data->mutex);
    if (job_data->status == 0) { // 0 means unclaimed file
      job_data->status = 1; // 1 means already claimed by a thread
      pthread_mutex_unlock(&job_data->mutex);
      read_file(job_data);
    } else {
      pthread_mutex_unlock(&job_data->mutex);
    }
  }
  return NULL;
}

File_list *list_dir(char *path) {
  DIR *dir = opendir(path);
  struct dirent *current_file;
  File_list *job_files_list = malloc(sizeof(File_list));
  job_files_list->num_files = 0;
  job_files_list->job_data = NULL;
  
  while ((current_file = readdir(dir)) != NULL) {
    process_entry(&job_files_list, current_file, path);
  }
  closedir(dir);
  return job_files_list;
}

void process_entry(File_list **job_files_list, struct dirent *current_file, char *path) {
  extern int max_concurrent_backups;
  if (current_file->d_type == 8 && strstr(current_file->d_name, ".job") != NULL) {
    Job_data *job_data = malloc(sizeof(Job_data));
    job_data->next = NULL;
    job_data->backup_counter = 1;
    job_data->status = 0;
    pthread_mutex_init(&job_data->mutex, NULL);
    size_t path_len = strlen(path) + strlen(current_file->d_name) + 2; // +2 for '/' and null terminator
    job_data->job_file_path = malloc(path_len * sizeof(char));
    snprintf(job_data->job_file_path, path_len, "%s/%s", path, current_file->d_name);
    add_job_data(job_files_list, job_data);
  } else if (current_file->d_type == 4 && strcmp(current_file->d_name, ".") != 0 && strcmp(current_file->d_name, "..") != 0) {
    char nested_path[PATH_MAX];
    snprintf(nested_path, sizeof(nested_path), "%s/%s", path, current_file->d_name);
    DIR *nested_dir = opendir(nested_path);
    if (nested_dir != NULL) {
      while ((current_file = readdir(nested_dir)) != NULL) {
        process_entry(job_files_list, current_file, nested_path);
      }
    }
    closedir(nested_dir);
  }
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
  (*job_files_list)->job_data = NULL; // After it clears the linked list the head is set to null
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
  }
  (*job_files_list)->num_files++;
}
