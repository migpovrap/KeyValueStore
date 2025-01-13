#ifndef JOBS_MANAGER_H
#define JOBS_MANAGER_H

#include <dirent.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

#include "macros.h"

typedef struct Job {
  char *job_file_path;
  int job_fd; //fd means file descriptor
  int job_output_fd;
  int backup_counter;
  struct Job *next;
} Job;

typedef struct JobQueue{
  int num_files;
  Job* current_job;
  Job* last_job;
  pthread_mutex_t queue_mutex;
} JobQueue;

/**
 * @brief Creates the job queue by reading entries from the specified directory.
 * 
 * @param dir_path The path to the directory containing job entries.
 * @return A pointer to the created JobQueue.
 */
JobQueue *create_job_queue(char *dir_path);

/**
 * @brief Processes files from the job queue.
 *
 * This function is executed by a thread to process files from the job queue.
 * It locks the queue mutex, retrieves the current job, and processes files
 * until the queue is empty. The function reads files and processes jobs
 * accordingly.
 *
 * @param arg A pointer to the job queue (JobQueue *).
 */
void *process_file(void *arg);

/**
 * @brief Destroys the job queue.
 * 
 * @param queue The job queue to destroy.
 */
void destroy_jobs_queue(JobQueue *queue);

#endif
