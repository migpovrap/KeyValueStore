#include "jobs_parser.h"
#include "operations.h"
#include "parser.h"

/**
 * @brief Initializes a Job structure with the given file path.
 *
 * @param job Pointer to the Job structure to be initialized.
 * @param file_path Path to the job file.
 */
void initialize_job(Job *job, const char *file_path) {
  job->job_file_path = strdup(file_path);
  job->job_fd = -1;
  job->job_output_fd = -1;
  job->backup_counter = 1;
  job->next = NULL;
}

/**
 * @brief Initializes the job queue.
 * 
 * @param queue The job queue to initialize.
 */
void initialize_jobs_queue(JobQueue *queue) {
  queue->current_job = NULL;
  queue->last_job = NULL;
  queue->num_files = 0;
  pthread_mutex_init(&queue->queue_mutex, NULL);
}

/**
 * @brief Adds a job to the queue.
 * 
 * @param queue The job queue.
 * @param job The job data to add.
 */
void enqueue_job(JobQueue *queue, Job *job) {
  pthread_mutex_lock(&queue->queue_mutex);
  job->next = NULL;
  if (queue->current_job == NULL) {
    queue->current_job = job;
  } else {
    Job *temp = queue->current_job;
    while (temp->next != NULL) {
      temp = temp->next;
    }
    temp->next = job;
  }
  queue->num_files++;
  pthread_mutex_unlock(&queue->queue_mutex);
}

/**
 * @brief Removes a job from the queue.
 * 
 * @param queue The job queue.
 * @return Job* The job data removed from the queue.
 */
Job* dequeue_job(JobQueue *queue) {
  pthread_mutex_lock(&queue->queue_mutex);
  if (queue->current_job == NULL) {
    pthread_mutex_unlock(&queue->queue_mutex);
    return NULL;
  }
  Job *job = queue->current_job;
  queue->current_job = queue->current_job->next;
  queue->num_files--;
  pthread_mutex_unlock(&queue->queue_mutex);
  return job;
}

/**
 * @brief Creates jobs and adds them to the job queue.
 * 
 * @param queue The job queue
 * @param current_file 
 * @param dir_path The path to the directory containing job entries.
 */
void create_jobs(JobQueue **queue, struct dirent *current_file,
char *dir_path) {
  if (current_file->d_type == 8 && strstr(current_file->d_name, ".job") != NULL) {
    Job *current_job = malloc(sizeof(Job));
    char *job_file_path = malloc(PATH_MAX);
    size_t path_len = (size_t) snprintf(job_file_path, PATH_MAX, "%s/%s",
    dir_path, current_file->d_name);
    job_file_path = realloc(job_file_path, path_len + 1); // +1 for null terminator
    initialize_job(current_job, job_file_path);
    free(job_file_path);
    enqueue_job(*queue, current_job);
  } else if (current_file->d_type == 4 && strcmp(current_file->d_name, ".") != 0\
  && strcmp(current_file->d_name, "..") != 0) {
    char *nested_path = malloc(PATH_MAX);
    size_t nested_path_len = (size_t) snprintf(nested_path, PATH_MAX, "%s/%s",
    dir_path, current_file->d_name);
    nested_path = realloc(nested_path, nested_path_len + 1); // +1 for null terminator
    DIR *nested_dir = opendir(nested_path);
    if (nested_dir != NULL)
      while ((current_file = readdir(nested_dir)) != NULL)
        create_jobs(queue, current_file, nested_path);
    closedir(nested_dir);
    free(nested_path);
  }
}

JobQueue *create_job_queue(char *dir_path) {
  DIR *dir = opendir(dir_path);
  struct dirent *current_file;
  JobQueue *queue = malloc(sizeof(JobQueue));
  initialize_jobs_queue(queue);
  while ((current_file = readdir(dir)) != NULL)
    create_jobs(&queue, current_file, dir_path);
  closedir(dir);
  return queue;
}

// Destructively sorts keys in alphabetical order using insertion sort.
void sort_keys_alphabetically(char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE],
size_t num_pairs) {
  for (size_t i = 1; i < num_pairs; ++i) {
    char temp[MAX_STRING_SIZE];
    strcpy(temp, (*keys)[i]);
    size_t j = i - 1;
    
    while (strcmp(temp, (*keys)[j]) < 0) {
      strcpy((*keys)[j + 1], (*keys)[j]);
      --j;
    }
    strcpy((*keys)[j + 1], temp);
  }
}

void cmd_write(Job* job, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE],
char (*values)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_write(job->job_fd, *keys, *values,
  MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0)
    fprintf(stderr, "Invalid command. See HELP for usage.\n");

  sort_keys_alphabetically(keys, num_pairs); // Sorting to avoid deadlocks.

  if (kvs_write(num_pairs, *keys, *values, job->job_output_fd))
    fprintf(stderr, "Failed to write pair.\n");
}

void cmd_read(Job* job, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_read_delete(job->job_fd, *keys,
  MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0)
    fprintf(stderr, "Invalid command. See HELP for usage.\n");
  
  sort_keys_alphabetically(keys, num_pairs); // Sorting to avoid deadlocks.

  if (kvs_read(num_pairs, *keys, job->job_output_fd))
    fprintf(stderr, "Failed to read pair.\n");
}

void cmd_delete(Job* job, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_read_delete(job->job_fd, *keys,
  MAX_WRITE_SIZE, MAX_STRING_SIZE);

  if (num_pairs == 0)
    fprintf(stderr, "Invalid command. See HELP for usage.\n");
  
  sort_keys_alphabetically(keys, num_pairs); // Sorting to avoid deadlocks.

  if (kvs_delete(num_pairs, *keys, job->job_output_fd))
    fprintf(stderr, "Failed to delete pair.\n");
}

void cmd_wait(Job* job) {
  unsigned int delay;

  if (parse_wait(job->job_fd, &delay, NULL) == -1)
    fprintf(stderr, "Invalid command. See HELP for usage.\n");

  if (delay > 0)
    kvs_wait(delay, job->job_output_fd);
}

void cmd_backup(Job* job, JobQueue* queue) {
  char *backup_out_file_path = malloc(PATH_MAX);
  strcpy(backup_out_file_path, job->job_file_path);
  char temp_path[PATH_MAX];
  strcpy(temp_path, backup_out_file_path);
  temp_path[strlen(temp_path) - 4] = '\0';
  size_t path_len = (size_t) snprintf(backup_out_file_path, PATH_MAX,
  "%s-%d.bck", temp_path, job->backup_counter);
  backup_out_file_path = realloc(backup_out_file_path, path_len + 1); // +1 for null terminator

  if (kvs_backup(backup_out_file_path, queue))
    fprintf(stderr, "Failed to perform backup.\n");

  job->backup_counter++;
  
  free(backup_out_file_path);
}

void read_file(Job* job, JobQueue* queue) {
  char *job_out_file_path = malloc(PATH_MAX);
  strcpy(job_out_file_path, job->job_file_path);
  char temp_path[PATH_MAX];
  strcpy(temp_path, job_out_file_path);
  temp_path[strlen(temp_path) - 3] = '\0';
  size_t path_len = (size_t) snprintf(job_out_file_path, PATH_MAX,
  "%sout", temp_path);
  job_out_file_path = realloc(job_out_file_path, path_len + 1); // +1 for null terminator
  job->job_fd = open(job->job_file_path, O_RDONLY);
  job->job_output_fd = open(job_out_file_path,
  O_WRONLY | O_CREAT | O_TRUNC, 0644);

  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};

  enum Command cmd;
  while ((cmd = get_next(job->job_fd)) != EOC) {
    switch (cmd) {
      case CMD_WRITE:
        cmd_write(job, &keys, &values);
        break;
      case CMD_READ:
        cmd_read(job, &keys);
        break;
      case CMD_DELETE:
        cmd_delete(job, &keys);
        break;
      case CMD_SHOW:
        kvs_show(job->job_output_fd);
        break;
      case CMD_WAIT:
        cmd_wait(job);
        break;
      case CMD_BACKUP:
        cmd_backup(job, queue);
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
  free(job_out_file_path);
  close(job->job_fd);
  close(job->job_output_fd);
}

/**
 * @brief Frees the memory allocated for a job.
 * 
 * @param job The job to be freed.
 */
void destroy_job(Job *job) {
  if(job != NULL) {
    free(job->job_file_path);
    free(job);
  }
}

void destroy_jobs_queue(JobQueue *queue) {
  pthread_mutex_lock(&queue->queue_mutex);
  queue->current_job = NULL;
  queue->num_files = 0;
  pthread_mutex_unlock(&queue->queue_mutex);
  pthread_mutex_destroy(&queue->queue_mutex);
  free(queue);
}

void *process_file(void *arg) {
  JobQueue *queue = (JobQueue *)arg;
  pthread_mutex_lock(&queue->queue_mutex);
  while (queue->num_files > 0) {
    pthread_mutex_unlock(&queue->queue_mutex);
    Job *job = dequeue_job(queue);
    read_file(job, queue);
    destroy_job(job);
    pthread_mutex_lock(&queue->queue_mutex);
  }
  pthread_mutex_unlock(&queue->queue_mutex);
  return NULL;
}
