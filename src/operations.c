#include "operations.h"
#include "kvs.h"
#include "jobs_parser.h"

static struct HashTable* hash_table = NULL;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init(int fd) {
  if (hash_table != NULL) {
    char buffer[PIPE_BUF];
    size_t buff_size = sizeof(buffer);
    size_t offset = 0;
    offset += (size_t)snprintf(buffer + offset, buff_size - offset,
    "KVS state has already been initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  hash_table = create_hash_table();
  return hash_table == NULL; // Checks if the HashTable was created successfully
}

int kvs_terminate(int fd) {
  if (hash_table == NULL) {
    char buffer[PIPE_BUF];
    size_t buff_size = sizeof(buffer);
    size_t offset = 0;
    offset += (size_t) snprintf(buffer + offset, buff_size - offset,
    "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  free_table(hash_table);
  return 0;
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], 
  char values[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  if (hash_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset,
    "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  for (size_t i = 0; i < num_pairs; i++)
    if (write_pair(hash_table, keys[i], values[i]) != 0)
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
  write(fd, buffer, offset);
  return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  if (hash_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset,
    "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "[");
  for (size_t i = 0; i < num_pairs; i++) {
    char* result = read_pair(hash_table, keys[i]);
    if (result == NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s,KVSERROR)", keys[i]);
    } else {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s,%s)", keys[i], result);
    }
    free(result);
  }
  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "]\n");
  write(fd, buffer, offset);
  return 0;
}

int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  if (hash_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset,
    "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  int aux = 0;
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
  if (aux) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "]\n");
  }
  write(fd, buffer, offset);
  return 0;
}

void kvs_show(int fd) {
  //FIXME The data that can be altered during its execution confirm with (Daniel Reis).
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  pthread_mutex_lock(&hash_table->table_mutex);
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *key_node = hash_table->table[i];
    while (key_node != NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset,
      "(%s, %s)\n", key_node->key, key_node->value);
      key_node = key_node->next;
    }
  }
  pthread_mutex_unlock(&hash_table->table_mutex);
  write(fd, buffer, offset);
}

void kvs_show_backup(int fd) {
  //FIXME A thread safe version, used for backups (Maybe use the same for both things, confirm with Daniel Reis)
  // This function needs to take a sanpshot of the hashtable for the backup.
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  pthread_mutex_lock(&hash_table->table_mutex);
  for (int i = 0; i < TABLE_SIZE; i++) {
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
  pthread_mutex_unlock(&hash_table->table_mutex);
  write(fd, buffer, offset);
}

void kvs_wait(unsigned int delay_ms, int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  offset += (size_t) snprintf(buffer + offset, buff_size - offset,
  "Waiting...\n");
  write(fd, buffer, offset);
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

int kvs_backup(char* backup_out_file_path, JobQueue* queue) {
  extern pthread_mutex_t backup_mutex;
  extern int concurrent_backups;
  extern int max_concurrent_backups;
  extern pid_t *backup_forks_pids;
  pid_t pid;
  while (1) {
    pthread_mutex_lock(&backup_mutex);
    if (concurrent_backups < max_concurrent_backups) {
      pthread_mutex_unlock(&backup_mutex);
      break;
    }
    pthread_mutex_unlock(&backup_mutex);
    pid_t exited_pid = wait(NULL);

    pthread_mutex_lock(&backup_mutex);
    for (int i = 0; i < concurrent_backups; ++i) {
      if (backup_forks_pids[i] == exited_pid) {
        for (int j = i; j < concurrent_backups - 1; ++j)
          backup_forks_pids[j] = backup_forks_pids[j + 1];
        concurrent_backups--;
        break;
      }
    }
    pthread_mutex_unlock(&backup_mutex);
  }

  pid = fork();
  
  if (pid < 0) {
    return 1; // Fork failed
  }

  if (pid == 0) {
    // This is the child process
    int backup_output_fd = open(backup_out_file_path,
    O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (backup_output_fd < 0) {
      _exit(EXIT_FAILURE); // Failed to open file
    }
    kvs_show_backup(backup_output_fd);
    close(backup_output_fd);
    free(backup_out_file_path);
    free(backup_forks_pids);
    destroy_jobs_queue(queue);
    kvs_terminate(STDERR_FILENO);
    _exit(EXIT_SUCCESS);
  }

  // This is the parent process
  pthread_mutex_lock(&backup_mutex);
  backup_forks_pids[concurrent_backups] = pid;
  concurrent_backups++;
  pthread_mutex_unlock(&backup_mutex);
  return 0;
}
