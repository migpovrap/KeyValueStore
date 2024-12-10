#include "operations.h"
#include "kvs.h"

static struct HashTable* kvs_table = NULL;

pthread_mutex_t backup_mutex = PTHREAD_MUTEX_INITIALIZER;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

int kvs_init(int fd) {
  if (kvs_table != NULL) {
    char buffer[PIPE_BUF];
    size_t buff_size = sizeof(buffer);
    size_t offset = 0;

    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state has already been initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  kvs_table = create_hash_table();
  return kvs_table == NULL; // Checks if the HashTable was created sucessfuly
}

int kvs_terminate(int fd) {
  if (kvs_table == NULL) {
    char buffer[PIPE_BUF];
    size_t buff_size = sizeof(buffer);
    size_t offset = 0;
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  free_table(kvs_table);
  return 0;
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  for (size_t i = 0; i < num_pairs; i++) {
    if (write_pair(kvs_table, keys[i], values[i]) != 0) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
    }
  }
  write(fd, buffer, offset);
  return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "[");
  for (size_t i = 0; i < num_pairs; i++) {
    char* result = read_pair(kvs_table, keys[i]);
    if (result == NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "(%s,KVSERROR)", keys[i]);
    } else {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "(%s,%s)", keys[i], result);
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
  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    write(fd, buffer, offset);
    return 1;
  }
  int aux = 0;
  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(kvs_table, keys[i]) != 0) {
      if (!aux) {
        offset += (size_t) snprintf(buffer + offset, buff_size - offset, "[");
        aux = 1;
      }
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "(%s,KVSMISSING)", keys[i]);
    }
  }
  if (aux) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "]\n");
  }
  write(fd, buffer, offset);
  return 0;
}

void kvs_show(int fd) {
  //TODO The data that can be alter during its execution confirm with (Daniel Reis).
  static pthread_mutex_t kvs_show_mutex = PTHREAD_MUTEX_INITIALIZER;
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  pthread_mutex_lock(&kvs_show_mutex);
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = kvs_table->table[i];
    while (keyNode != NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "(%s, %s)\n", keyNode->key, keyNode->value);
      keyNode = keyNode->next;
    }
  }
  pthread_mutex_unlock(&kvs_show_mutex);
  write(fd, buffer, offset);
}

void kvs_show_backup(int fd) {
  //FIXME A thread safe version, used for backups (Maybe use the same for both things, confirm with Daniel Reis)
  // This function needs to take a sanpshot of the hashtable for the backup.
  static pthread_mutex_t kvs_show_mutex = PTHREAD_MUTEX_INITIALIZER;
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  pthread_mutex_lock(&kvs_show_mutex);
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = kvs_table->table[i];
    while (keyNode != NULL) {
      size_t len_key = strlen(keyNode -> key);
      size_t len_value = strlen(keyNode -> value);
      size_t line_len = len_key + len_value + 5; // For (, )\n
      if (offset + line_len < buff_size) {
        buffer[offset++] = '(';
        memcpy(buffer + offset, keyNode -> key, len_key);
        offset += len_key;
        memcpy(buffer + offset, ", ", 2);
        offset += 2;
        memcpy(buffer + offset, keyNode -> value, len_value);
        offset += len_value;
        memcpy(buffer + offset, ")\n", 2);
        offset += 2;
        buffer[offset] = '\0';
      }  
      keyNode = keyNode->next;
    }
  }
  pthread_mutex_unlock(&kvs_show_mutex);
  write(fd, buffer, offset);
}

void kvs_wait(unsigned int delay_ms, int fd) {
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "Waiting...\n");
  write(fd, buffer, offset);
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

void kvs_backup(int backupoutput) {
  static int concurrent_backups = 0;
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
    fprintf(stderr, "Reached the maximum of concurrent forks,  waiting for a fork to exit.\n"); //REMOVE
    pid_t exited_pid = wait(NULL);

    pthread_mutex_lock(&backup_mutex);
    // Removes the fork pid from the backups pid array
    for (int i = 0; i < concurrent_backups; ++i) {
      if (backup_forks_pids[i] == exited_pid) {
        // Shift the remaining elements to fill the gap
        for (int j = i; j < concurrent_backups - 1; ++j) {
          backup_forks_pids[j] = backup_forks_pids[j + 1];
        }
        --concurrent_backups;
        break;
      }
    }
    pthread_mutex_unlock(&backup_mutex);
  }

  pid = fork();

  if (pid == 0) {
    // This is the child process
    printf("Fork launched new child process, performing backup.\n"); //REMOVE
    kvs_show_backup(backupoutput);
    close(backupoutput);
    printf("Backup completed, child process, terminated.\n"); //REMOVE
    exit(EXIT_SUCCESS);
  }

  // This is the parent process
  pthread_mutex_lock(&backup_mutex);
  backup_forks_pids[concurrent_backups] = pid;
  ++concurrent_backups;
  pthread_mutex_unlock(&backup_mutex);
  fprintf(stderr, "Process created a fork, return to read_file() function.\n"); //REMOVE
  return; // Continues executing from the call function read_file()
}
