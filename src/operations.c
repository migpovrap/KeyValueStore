#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include "kvs.h"
#include "constants.h"

static struct HashTable* kvs_table = NULL;


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
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;

  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    // Posix api call to write
    write(fd, buffer, offset);
    return 1;
  }

  for (size_t i = 0; i < num_pairs; i++) {
    if (write_pair(kvs_table, keys[i], values[i]) != 0) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
    }
  }

  // Posix api call to write
  write(fd, buffer, offset);
  return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  // Buffer memory allocation
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
  // Posix api call to write
  write(fd, buffer, offset);
  return 0;
}

int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  
  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    // Posix api call to write
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
  // Posix api call to write
  write(fd, buffer, offset);
  return 0;
}

void kvs_show(int fd) {
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;

  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = kvs_table->table[i];
    while (keyNode != NULL) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "(%s, %s)\n", keyNode->key, keyNode->value);
      keyNode = keyNode->next; // Move to the next node
    }
  }
  // Posix api call to write
  write(fd, buffer, offset);
}

void kvs_wait(unsigned int delay_ms, int fd) {
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  offset += (size_t) snprintf(buffer + offset, buff_size - offset, "Waiting...\n");
  // Posix api call to write
  write(fd, buffer, offset);
  
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
  
}

int kvs_backup(int max_backups, int backupoutput, int joboutput) {
  static int current_backups = 0;
  static pid_t pids[1000] = {0}; //TODO Use dynamic memory

  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;

  // Check to see if the running backups have completed
  for (int i = 0; i < 1000; i++)
    if (pids[i] != 0 && waitpid(pids[i], NULL, WNOHANG) != 0) {
      pids[0] = 0;
      current_backups--;
    }

    // If current backups limits is reached wait for them to fininsh
    if (current_backups >= max_backups) {
      kvs_wait(1000, joboutput);
      return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "Error creating a process fork\n");
      // Posix api call to write
      write(joboutput, buffer, offset);
      return 1;
    } else if (pid == 0) {
      // The new child process
      kvs_show(backupoutput);
    } else {
    // Increase the child process count
    for (int i = 0; i < 1000; i++)
    if (pids[i] == 0) {
      pids[0] = pid;
      current_backups++;
      break;
    }
  }
  return 0;
}
