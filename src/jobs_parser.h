#ifndef JOBS_PARSER_H
#define JOBS_PARSER_H

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>

// Fallback definition for DT_REG if not defined
#ifndef DT_REG
#define DT_REG 8
#endif

typedef struct {
    char **path_job_files;
    int num_files;
} File_list;

/**
 * @brief List all the files in a dir and for each .job call the read_file() function
 * 
 * @param path The path of the directory containing the job files can be releative or complete
 */
File_list *list_dir(char *path);

/**
 * @brief Reads the job file from the specified path.
 * 
 * @param job_file_path Complete path to the job file.
 * @param max_backups The maximun number of concurrent backups process
 */
void read_file(char *job_file_path, int max_backups);

/**
 * @brief Writes a command to the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 * @param keys Array of keys to write.
 * @param values Array of values to write.
 * @param joboutput File descriptor of the pretended output a .out file
 */
void cmd_write(int *jobfd, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], char (*values)[MAX_WRITE_SIZE][MAX_STRING_SIZE], int *joboutput);

/**
 * @brief Reads a command from the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 * @param keys Array of keys to read.
 * @param joboutput File descriptor of the pretended output a .out file
 */
void cmd_read(int *jobfd, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], int *joboutput);

/**
 * @brief Deletes a command from the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 * @param keys Array of keys to delete.
 * @param joboutput File descriptor of the pretended output a .out file
 */
void cmd_delete(int *jobfd, char (*keys)[MAX_WRITE_SIZE][MAX_STRING_SIZE], int *joboutput);

/**
 * @brief Waits for a command to complete on the job file descriptor.
 * 
 * @param jobfd File descriptor of the .job file
 */
void cmd_wait(int *jobfd, int *joboutput);

/**
 * @brief Calls the kvs_backup with the current backup and gives an error in case of failure
 * 
 * @param max_backups The maximun number of concurrent backups process
 * @param backupoutput File descriptor of the .bck file
 * @param joboutput File descriptor of the .job file
 */
void cmd_backup(int max_backups, int *backup_counter, char *job_file_path);
#endif