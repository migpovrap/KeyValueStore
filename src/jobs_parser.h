#ifndef JOBS_PARSER_H
#define JOBS_PARSER_H

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>

#include "constants.h"

typedef struct Job_data {
  int job_fd;
  char *job_file_path;
  int job_output_fd;
  int backup_counter;
  int status;
  pthread_mutex_t mutex;
  struct Job_data *next;
} Job_data;

typedef struct {
  Job_data* job_data;
  int num_files;
} File_list;

/**
 * @brief List all the files in a dir and for each .job call the read_file() function
 * 
 * @param path The path of the directory containing the job files can be releative or complete
 */
File_list *list_dir(char *path);

/**
 * @brief Process the entry of the directory and add the job data to the list
 * 
 * @param job_files_list The list of job data
 * @param current_file The current file to process
 * @param path The path of the directory containing the job files can be releative or complete
 */
void process_entry(File_list **job_files_list, struct dirent *current_file, char *path);

/**
 * @brief Process the file and execute the commands in it
 * 
 * @param arg The file path and the max number of backups
 */
void *process_file(void *arg);

/**
 * @brief Reads the job file from the specified path.
 * 
 * @param job_file_path Complete path to the job file.
 * @param max_backups The maximun number of concurrent backups process
 */
void read_file(Job_data* job_data);

/**
 * @brief Writes a command to the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 * @param keys Array of keys to write.
 * @param values Array of values to write.
 * @param joboutput File descriptor of the pretended output a .out file
 */
void cmd_write(Job_data* job_data, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], char (*values)[MAX_WRITE_SIZE][MAX_STRING_SIZE]);

/**
 * @brief Reads a command from the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 * @param keys Array of keys to read.
 * @param joboutput File descriptor of the pretended output a .out file
 */
void cmd_read(Job_data* job_data, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]);

/**
 * @brief Deletes a command from the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 * @param keys Array of keys to delete.
 * @param joboutput File descriptor of the pretended output a .out file
 */
void cmd_delete(Job_data* job_data, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE]);

/**
 * @brief Waits for a command to complete on the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 */
void cmd_wait(Job_data* job_data);

/**
 * @brief Calls the kvs_backup with the current backup and gives an error in case of failure
 * 
 * @param max_backups The maximun number of concurrent backups process
 * @param backupoutput File descriptor of the .bck file
 * @param joboutput File descriptor of the .job file
 */
void cmd_backup(Job_data* job_data);

/**
 * @brief Clears the job data list.
 * 
 * @param job_files_list The list of job data
 */
void clear_job_data_list(File_list** job_files_list);

/**
 * @brief Adds a new job data to the list.
 * 
 * @param job_files_list The list of job data
 * @param new_job_data The new job data to add
 */
void add_job_data(File_list** job_files_list, Job_data* new_job_data);

#endif