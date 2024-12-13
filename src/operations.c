#include "operations.h"
#include "kvs.h"
#include "jobs_parser.h"
#include "macros.h"

static struct HashTable* hash_table = NULL;

struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init() {
  CHECK_NOT_NULL(hash_table, "KVS state has already been initialized.");

  hash_table = create_hash_table();
  return hash_table == NULL; // Checks if the HashTable was created successfully
}

int kvs_terminate() {
  CHECK_NULL(hash_table, "KVS state must be initialized.");

  free_table(hash_table);
  hash_table = NULL;
  return 0;
}

/**
 * @brief Locks or unlocks hash table entries based on the provided keys and lock type.
 *
 * Ensures locks are always locked and unlocked in a specific order to avoid deadlocks.
 * 
 * @param keys An array of strings representing the keys to be locked or unlocked.
 * @param num_pairs The number of key-value pairs.
 * @param type The type of lock operation to perform (READ_LOCK, WRITE_LOCK, READ_UNLOCK, WRITE_UNLOCK)
 */
void lock_unlock_hashes(char keys[][MAX_STRING_SIZE], size_t num_pairs, LOCK_TYPE type) {
  int HASH_LOCK_BITMAP[TABLE_SIZE] = {0};
  
  for (size_t i = 0; i < num_pairs; ++i)
    HASH_LOCK_BITMAP[hash(keys[i])] = 1;

  for (int i = 0; i < TABLE_SIZE; ++i)
    if (HASH_LOCK_BITMAP[i])
      switch (type) {
        case READ_LOCK:
          pthread_rwlock_rdlock(&hash_table->hash_lock[i]);
          break;
        case WRITE_LOCK:
          pthread_rwlock_wrlock(&hash_table->hash_lock[i]);
          break;
        case READ_UNLOCK:
          pthread_rwlock_unlock(&hash_table->hash_lock[i]);
          break;
        case WRITE_UNLOCK:
          pthread_rwlock_unlock(&hash_table->hash_lock[i]);
          break;
      }
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], 
char values[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  CHECK_NULL(hash_table, "KVS state must be initialized.");

  lock_unlock_hashes(keys, num_pairs, WRITE_LOCK);

  for (size_t i = 0; i < num_pairs; ++i)
    if (write_pair(hash_table, keys[i], values[i]) != 0)
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
  
  lock_unlock_hashes(keys, num_pairs, WRITE_UNLOCK);

  CHECK_RETURN_MINUS_ONE(write(fd, buffer, offset), "Error during writing.");
  return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  CHECK_NULL(hash_table, "KVS state must be initialized.");

  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "[");

  lock_unlock_hashes(keys, num_pairs, READ_LOCK);

  for (size_t i = 0; i < num_pairs; ++i) {
    char* result = read_pair(hash_table, keys[i]);
    if (result == NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s,KVSERROR)", keys[i]);
    } else {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s,%s)", keys[i], result);
    }
    free(result);
    result = NULL;
  }

  lock_unlock_hashes(keys, num_pairs, READ_UNLOCK);

  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "]\n");
  CHECK_RETURN_MINUS_ONE(write(fd, buffer, offset), "Error during writing.");
  return 0;
}

int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  CHECK_NULL(hash_table, "KVS state must be initialized.");

  int aux = 0;

  lock_unlock_hashes(keys, num_pairs, WRITE_LOCK);
  
  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(hash_table, keys[i]) != 0) {
      if (!aux) {
        offset += (size_t) snprintf(buffer + offset, buff_size - offset, "[");
        aux = 1;
      }
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s,KVSMISSING)", keys[i]);
    }
  }

  lock_unlock_hashes(keys, num_pairs, WRITE_UNLOCK);

  if (aux)
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "]\n");

  CHECK_RETURN_MINUS_ONE(write(fd, buffer, offset), "Error during writing.");
  return 0;
}

void kvs_show(int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;

  for (int i = 0; i < TABLE_SIZE; i++)
    pthread_rwlock_rdlock(&hash_table->hash_lock[i]);

  for (int i = 0; i < TABLE_SIZE; ++i) {
    KeyNode *key_node = hash_table->table[i];
    while (key_node != NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s, %s)\n", key_node->key, key_node->value);
      key_node = key_node->next;
    }
  }

  for (int i = 0; i < TABLE_SIZE; ++i)
    pthread_rwlock_unlock(&hash_table->hash_lock[i]);

  CHECK_RETURN_MINUS_ONE(write(fd, buffer, offset), "Error during writing.");
}

/**
 * @brief Thread-safe version of kvs_show using memcpy that writes the contents of the hash table to a file descriptor.
 *
 * This function iterates through the hash table and writes each key-value pair to the provided file descriptor
 * in the format "(key, value)\n". It ensures that the buffer does not overflow by checking the available space
 * before writing each key-value pair.
 *
 * @param fd The file descriptor to which the hash table contents will be written.
 */
void kvs_show_backup(int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  for (int i = 0; i < TABLE_SIZE; ++i) {
    KeyNode *key_node = hash_table->table[i];
    while (key_node != NULL) {
      size_t len_key = strlen(key_node -> key);
      size_t len_value = strlen(key_node -> value);
      size_t line_len = len_key + len_value + 5; // For (, )\n
      if (offset + line_len < buff_size) {
        buffer[offset++] = '(';
        memcpy(buffer + offset, key_node -> key, len_key);
        offset += len_key;
        memcpy(buffer + offset, ", ", 2);
        offset += 2;
        memcpy(buffer + offset, key_node -> value, len_value);
        offset += len_value;
        memcpy(buffer + offset, ")\n", 2);
        offset += 2;
        buffer[offset] = '\0';
      }  
      key_node = key_node->next;
    }
  }
  CHECK_RETURN_MINUS_ONE(write(fd, buffer, offset), "Error during writing.");
}

void kvs_wait(unsigned int delay_ms, int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  offset += (size_t) snprintf(buffer + offset, buff_size - offset,
  "Waiting...\n");
  CHECK_RETURN_MINUS_ONE(write(fd, buffer, offset), "Error during writing.");
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

int kvs_backup(char* backup_out_file_path, JobQueue* queue) {
  extern sem_t backup_semaphore;
  pid_t pid;

  sem_wait(&backup_semaphore);

  pid = fork();
  if (pid < 0) return 1; // Fork failed

  if (pid == 0) {
    // This is the child process
    int backup_output_fd = open(backup_out_file_path,
    O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (backup_output_fd < 0) // Failed to open file
      _exit(EXIT_FAILURE);
    kvs_show_backup(backup_output_fd);
    close(backup_output_fd);
    free(backup_out_file_path);
    backup_out_file_path = NULL;
    destroy_jobs_queue(queue);
    queue = NULL;
    kvs_terminate();
    _exit(EXIT_SUCCESS);
  }
  return 0;
}

void signal_child_terminated() {
  extern _Atomic int child_terminated;
  atomic_store(&child_terminated, 1);
}

void* checks_for_terminated_chlidren() {
  extern sem_t backup_semaphore;
  extern _Atomic int child_terminated;
    while (atomic_load(&child_terminated) != -1) {
      if (atomic_load(&child_terminated) == 1) {
        atomic_store(&child_terminated, 0);
          while (waitpid(-1, NULL, WNOHANG) > 0)
            sem_post(&backup_semaphore);
      }
      struct timespec delay = delay_to_timespec(1);
      nanosleep(&delay, NULL);
    }      
  return NULL;
}
