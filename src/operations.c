#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include "kvs.h"
#include "constants.h"
#include <pthread.h>


static struct HashTable* kvs_table = NULL;

//TODO Change this mutex to only lock each position in the hashtable not all the table (sync acess to hashtable)
pthread_mutex_t kvs_mutex = PTHREAD_MUTEX_INITIALIZER;
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
  pthread_mutex_lock(&kvs_mutex);
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;

  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    // Posix api call to write
    write(fd, buffer, offset);
    pthread_mutex_unlock(&kvs_mutex);
    return 1;
  }

  for (size_t i = 0; i < num_pairs; i++) {
    if (write_pair(kvs_table, keys[i], values[i]) != 0) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
    }
  }

  // Posix api call to write
  write(fd, buffer, offset);
  pthread_mutex_unlock(&kvs_mutex);
  return 0;
}

int kvs_read(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  pthread_mutex_lock(&kvs_mutex);
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;

  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    write(fd, buffer, offset);
    pthread_mutex_unlock(&kvs_mutex);
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
  pthread_mutex_unlock(&kvs_mutex);
  return 0;
}

int kvs_delete(size_t num_pairs, char keys[][MAX_STRING_SIZE], int fd) {
  pthread_mutex_lock(&kvs_mutex);
  // Buffer memory allocation
  char buffer[PIPE_BUF];
  size_t buff_size = sizeof(buffer);
  size_t offset = 0;
  
  if (kvs_table == NULL) {
    offset += (size_t) snprintf(buffer + offset, buff_size - offset, "KVS state must be initialized\n");
    // Posix api call to write
    write(fd, buffer, offset);
    pthread_mutex_unlock(&kvs_mutex);
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
  pthread_mutex_unlock(&kvs_mutex);
  return 0;
}

void kvs_show(int fd) {
  pthread_mutex_lock(&kvs_mutex);
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
  pthread_mutex_unlock(&kvs_mutex);
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

void kvs_backup(int max_backups, int backupoutput) {
  static int concurrent_backups = 0;
  pid_t pid;

  while (1) {
    pthread_mutex_lock(&backup_mutex);
    if (concurrent_backups < max_backups) {
      pthread_mutex_unlock(&backup_mutex);
      break;
    }
    pthread_mutex_unlock(&backup_mutex);
    fprintf(stderr, "Reached the maximum of concurrent forks,  waiting for a fork to exit.\n"); //REMOVE
    wait(NULL);

    pthread_mutex_lock(&backup_mutex);
    --concurrent_backups;
    pthread_mutex_unlock(&backup_mutex);
  }

  pid = fork();

  if (pid == -1) {
    // Fork cannot be created
    perror("Error creating the fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) { // Only runs if the fork (child process was sucessfuly launched)
    // This is the child process
    printf("Fork launched new child process, performing backup.\n"); //REMOVE
    kvs_show(backupoutput);
    close(backupoutput);
    printf("Backup completed, child process, terminated.\n"); //REMOVE
    exit(EXIT_SUCCESS);
  }

  // This is the parent process
  pthread_mutex_lock(&backup_mutex);
  ++concurrent_backups;
  pthread_mutex_unlock(&backup_mutex);
  fprintf(stderr, "Process created a fork, return to read_file() function.\n"); //REMOVE
  return; // Continues executing from the call function read_file()
}
