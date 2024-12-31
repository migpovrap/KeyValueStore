#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <stddef.h>

#include "constants.h"

/// Initializes the KVS state.
/// @return 0 if the KVS state was initialized successfully, 1 otherwise.
int kvs_init();

/// Destroys the KVS state.
/// @return 0 if the KVS state was terminated successfully, 1 otherwise.
int kvs_terminate();

/// Writes a key value pair to the KVS. If key already exists it is updated.
/// @param num_pairs Number of pairs being written.
/// @param keys Array of keys' strings.
/// @param values Array of values' strings.
/// @return 0 if the pairs were written successfully, 1 otherwise.
int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE],
char values[][MAX_STRING_SIZE]);

/// Reads values from the KVS.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @param fd File descriptor to write the (successful) output.
/// @return 0 if the key reading, 1 otherwise.
int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd);

/// Deletes key value pairs from the KVS.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @return 0 if the pairs were deleted successfully, 1 otherwise.
int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd);

/// Checks if a given key exists in the database.
/// @param key The key to be checked.
/// @return 1 if the key exists, 0 otherwise.
int key_exists(const char *key);

/// Writes the state of the KVS.
/// @param fd File descriptor to write the output.
void kvs_show(int fd);

/// Creates a backup of the KVS state and stores it in a backup file.
/// @param num_backup The number of the backup to be created.
/// @param job_filename The name of the file where the backup will be stored.
/// @param directory The directory where the backup file will be saved.
/// @return 0 if the backup was successful, 1 otherwise.
int kvs_backup(size_t num_backup,char* job_filename , char* directory);

/// Waits for a given amount of time.
/// @param delay_us Delay in milliseconds.
void kvs_wait(unsigned int delay_ms);

#endif  // OPERATIONS_H
