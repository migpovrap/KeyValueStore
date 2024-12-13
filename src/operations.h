#ifndef KVS_OPERATIONS_H
#define KVS_OPERATIONS_H

#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>

#include "constants.h"
#include "jobs_parser.h"

typedef enum {
  READ_LOCK,
  WRITE_LOCK,
  READ_UNLOCK,
  WRITE_UNLOCK
} LOCK_TYPE;

/**
 * Calculates a timespec from a delay in milliseconds.
 * @param delay_ms Delay in milliseconds.
 * @return Timespec with the given delay.
 */
struct timespec delay_to_timespec(unsigned int delay_ms);

/** 
 * Initializes the KVS state.
 * @return 0 if the KVS state was initialized successfully, 1 otherwise.
 */
int kvs_init();

/** 
 * Destroys the KVS state.
 * @return 0 if the KVS state was terminated successfully, 1 otherwise.
 */
int kvs_terminate();

/** 
 * Writes key-value pairs to the KVS. If a key already exists, it is updated.
 * @param num_pairs Number of pairs being written.
 * @param keys Array of keys' strings.
 * @param values Array of values' strings.
 * @param fd The file descriptor to write to.
 * @return 0 if the pairs were written successfully, 1 otherwise.
 */
int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE],
char values[][MAX_STRING_SIZE], int fd);

/** 
 * Reads values from the KVS.
 * @param num_pairs Number of pairs to read.
 * @param keys Array of keys' strings.
 * @param fd The file descriptor to write to.
 * @return 0 if the key reading was successful, 1 otherwise.
 */
int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd);

/** 
 * Deletes key-value pairs from the KVS.
 * @param num_pairs Number of pairs to delete.
 * @param keys Array of keys' strings.
 * @param fd The file descriptor to write to.
 * @return 0 if the pairs were deleted successfully, 1 otherwise.
 */
int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd);

/** 
 * Writes the state of the KVS to the specified file descriptor.
 * @param fd The file descriptor to write to.
 */
void kvs_show(int fd);

/** 
 * Waits for a given amount of time.
 * @param delay_ms Delay in milliseconds.
 * @param fd The file descriptor to write to.
 */
void kvs_wait(unsigned int delay_ms, int fd);

/** 
 * Creates a backup of the KVS state and stores it in the specified backup file.
 * @param backup_out_file_path Path to the backup output file.
 * @param queue Pointer to the job queue.
 * @return 0 if the backup was successful, 1 otherwise.
 */
int kvs_backup(char* backup_out_file_path, JobQueue* queue);

/**
 * @brief Auxiliary function to handle semaphore operations.
 * 
 * Checks for terminated child processes and when it finds one it increments (unlocks) the semaphore.
 */
void signal_child_terminated();

void* checks_for_terminated_chlidren();

#endif  // KVS_OPERATIONS_H
