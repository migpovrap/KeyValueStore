#include "jobs_parser.h"
#include "operations.h"
#include "parser.h"
#include "macros.h"

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
  job->backup_counter = 1; // Since naming scheme for backups starts at 1.
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
    while (temp->next != NULL)
      temp = temp->next;
    temp->next = job;
  }
  queue->num_files++;
  pthread_mutex_unlock(&queue->queue_mutex);
}

/**
 * @brief Removes a job from the queue.
 * 
 * @param queue The job queue.
 * @return Job* The job data removed from the queue, or NULL if the queue is empty.
 *
 * @return Job* The job data removed from the queue.
 */
Job* dequeue_job(JobQueue *queue) {
  if (queue->current_job == NULL) {
    return NULL;
  }
  Job *job = queue->current_job;
  queue->current_job = queue->current_job->next;
  queue->num_files--;
  return job;
}

/**
 * @brief Recursively creates jobs from files in a directory and adds them to the job queue.
 * 
 * This function checks if the current file is a job file or a directory. If it's a job file,
 * it initializes a Job structure and enqueues it. If it's a directory, it recursively processes
 * the directory to find more job files.
 * 
 * @param queue Pointer to the job queue.
 * @param current_file Pointer to the current directory entry.
 * @param dir_path Path to the directory containing job entries.
 */
void create_jobs(JobQueue **queue, struct dirent *current_file,
char *dir_path) {
  if (current_file->d_type == 8 && strstr(current_file->d_name, ".job") != NULL) {
    Job *current_job = malloc(sizeof(Job));
    CHECK_NULL(current_job, "Failed to alloc memory for job.");
    char *job_file_path = malloc(PATH_MAX);
    CHECK_NULL(job_file_path, "Failed to alloc memory for job file path.");
    size_t path_len = (size_t) snprintf(job_file_path, PATH_MAX, "%s/%s",
    dir_path, current_file->d_name);
    job_file_path = realloc(job_file_path, path_len + 1); // +1 for null terminator
    initialize_job(current_job, job_file_path);
    free(job_file_path);
    job_file_path = NULL;
    enqueue_job(*queue, current_job);
  } else if (current_file->d_type == 4 && strcmp(current_file->d_name, ".") != 0\
  && strcmp(current_file->d_name, "..") != 0) {
    char *nested_path = malloc(PATH_MAX);
    CHECK_NULL(nested_path, "Failed to alloc memory for job file path.");
    size_t nested_path_len = (size_t) snprintf(nested_path, PATH_MAX, "%s/%s",
    dir_path, current_file->d_name);
    nested_path = realloc(nested_path, nested_path_len + 1); // +1 for null terminator
    DIR *nested_dir = opendir(nested_path);
    CHECK_NULL(nested_dir, "Failed to open sub-directory.");
    if (nested_dir != NULL)
      while ((current_file = readdir(nested_dir)) != NULL)
        create_jobs(queue, current_file, nested_path); // Recursive call
    closedir(nested_dir);
    free(nested_path);
    nested_path = NULL;
  }
}

JobQueue *create_job_queue(char *dir_path) {
  DIR *dir = opendir(dir_path);
  CHECK_NULL(dir, "Failed to open directory.");
  struct dirent *current_file;
  JobQueue *queue = malloc(sizeof(JobQueue));
  initialize_jobs_queue(queue);
  while ((current_file = readdir(dir)) != NULL)
    create_jobs(&queue, current_file, dir_path);
  closedir(dir);
  return queue;
}

/**
 * @brief Handles the write command for a job.
 *
 * This function parses the write command from the job's file descriptor,
 * extracts the key-value pairs, and writes them to the key-value store.
 *
 * @param job Pointer to the Job structure containing job details.
 * @param keys Pointer to a 2D array to store the keys.
 * @param values Pointer to a 2D array to store the values.
 */
void cmd_write(Job* job, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE],
char (*values)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_write(job->job_fd, *keys, *values,
  MAX_WRITE_SIZE, MAX_STRING_SIZE);

  CHECK_NUM_PAIRS(num_pairs, "Invalid command. See HELP for usage.");

  CHECK_RETURN_ONE(kvs_write(num_pairs, *keys, *values, job->job_output_fd),
  "Failed to write pair.");
}

/**
 * @brief Reads key-value pairs from a job file descriptor and processes them.
 *
 * This function reads key-value pairs from the job's file descriptor, parses them,
 * and attempts to read the corresponding values from the key-value store.
 *
 * @param job A pointer to the Job structure containing job-related information.
 * @param keys A pointer to an array where the parsed keys will be stored.
 */
void cmd_read(Job* job, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_read_delete(job->job_fd, *keys,
  MAX_WRITE_SIZE, MAX_STRING_SIZE);

  CHECK_NUM_PAIRS(num_pairs, "Invalid command. See HELP for usage.");

  CHECK_RETURN_ONE(kvs_read(num_pairs, *keys, job->job_output_fd),
  "Failed to read pair.");
}

/**
 * @brief Deletes key-value pairs based on the provided job.
 *
 * This function parses the keys from the job's file descriptor and attempts to delete
 * the corresponding key-value pairs from the key-value store. If the command is invalid
 * or if the deletion fails, appropriate error messages are printed to stderr.
 *
 * @param job Pointer to the Job structure containing job details.
 * @param keys Pointer to a 2D array where parsed keys will be stored.
 */
void cmd_delete(Job* job, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]) {
  size_t num_pairs;
  num_pairs = parse_read_delete(job->job_fd, *keys,
  MAX_WRITE_SIZE, MAX_STRING_SIZE);

  CHECK_NUM_PAIRS(num_pairs, "Invalid command. See HELP for usage.");

  CHECK_RETURN_ONE(kvs_delete(num_pairs, *keys, job->job_output_fd),
  "Failed to delete pair.");
}

/**
 * @brief Executes a wait command for a job.
 *
 * This function parses the wait command from the job's file descriptor,
 * retrieves the delay value, and writes waiting to the job's output file.
 *
 * @param job Pointer to the Job structure containing job details.
 */
void cmd_wait(Job* job) {
  unsigned int delay;

  CHECK_RETURN_MINUS_ONE(parse_wait(job->job_fd, &delay, NULL),
  "Invalid command. See HELP for usage.");

  if (delay > 0)
    kvs_wait(delay, job->job_output_fd);
}

/**
 * @brief Performs a backup of the job file and increments the backup counter.
 *
 * This function creates a backup of the job file specified in the Job structure.
 * The backup file is named by appending the backup counter to the original file name.
 * The backup counter is then incremented.
 *
 * @param job A pointer to the Job structure containing the job file path and backup counter.
 * @param queue A pointer to the JobQueue structure used for the backup operation.
 */
void cmd_backup(Job* job, JobQueue* queue) {
  char *backup_out_file_path = malloc(PATH_MAX);
  CHECK_NULL(backup_out_file_path, "Failed to allocate memory for backup file.");
  strcpy(backup_out_file_path, job->job_file_path);
  char temp_path[PATH_MAX];
  strcpy(temp_path, backup_out_file_path);
  temp_path[strlen(temp_path) - 4] = '\0';
  size_t path_len = (size_t) snprintf(backup_out_file_path, PATH_MAX,
  "%s-%d.bck", temp_path, job->backup_counter);
  backup_out_file_path = realloc(backup_out_file_path, path_len + 1); // +1 for null terminator

  CHECK_RETURN_ONE(kvs_backup(backup_out_file_path, queue), "Failed to perform backup.");

  job->backup_counter++;
  free(backup_out_file_path);
  backup_out_file_path = NULL;
}

/**
 * @brief Reads a job file and processes commands, writing output to a specified file.
 *
 * This function reads commands from a job file and processes them accordingly.
 * It supports various commands such as write, read, delete, show, wait, and backup.
 * The output of the commands is written to a specified output file.
 *
 * @param job A pointer to the Job structure containing job details.
 * @param queue A pointer to the JobQueue structure for managing job backups.
 */
void read_file(Job* job, JobQueue* queue) {
  char *job_out_file_path = malloc(PATH_MAX);
  CHECK_NULL(job_out_file_path, "Failed to allocate memory for job output file.");
  strcpy(job_out_file_path, job->job_file_path);
  char temp_path[PATH_MAX];
  strcpy(temp_path, job_out_file_path);
  temp_path[strlen(temp_path) - 3] = '\0';
  size_t path_len = (size_t) snprintf(job_out_file_path, PATH_MAX,
  "%sout", temp_path);
  job_out_file_path = realloc(job_out_file_path, path_len + 1); // +1 for null terminator
  job->job_fd = open(job->job_file_path, O_RDONLY);
  CHECK_RETURN_MINUS_ONE(job->job_fd, "Failed to open job file.");
  job->job_output_fd = open(job_out_file_path,
  O_WRONLY | O_CREAT | O_TRUNC, 0644);
  CHECK_RETURN_MINUS_ONE(job->job_output_fd, "Failed to open job output file.");

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
  close(job->job_fd);
  close(job->job_output_fd);
  free(job_out_file_path);
  job_out_file_path = NULL;
}

/**
 * @brief Frees the memory allocated for a job.
 * 
 * @param job The job to be freed.
 */
void destroy_job(Job *job) {
  if(job != NULL) {
    free(job->job_file_path);
    job->job_file_path = NULL;
    free(job);
    job = NULL;
  }
}

void destroy_jobs_queue(JobQueue *queue) {
  pthread_mutex_lock(&queue->queue_mutex);
  queue->current_job = NULL;
  queue->num_files = 0;
  pthread_mutex_unlock(&queue->queue_mutex);
  pthread_mutex_destroy(&queue->queue_mutex);
  free(queue);
  queue = NULL;
}

void *process_file(void *arg) {
  JobQueue *queue = (JobQueue *)arg;
  pthread_mutex_lock(&queue->queue_mutex);
  while (queue->num_files > 0) {
    Job *job = dequeue_job(queue);
    pthread_mutex_unlock(&queue->queue_mutex);
    read_file(job, queue);
    destroy_job(job);
    job = NULL;
    pthread_mutex_lock(&queue->queue_mutex);
  }
  pthread_mutex_unlock(&queue->queue_mutex);
  return NULL;
}
