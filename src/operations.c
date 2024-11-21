#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>

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
  return kvs_table == NULL;
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
    write(fd, buffer, offset);
    return 1;
  }

  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(kvs_table, keys[i]) != 0) {
      offset += (size_t) snprintf(buffer + offset, buff_size - offset, "(%s,KVSMISSING)\n", keys[i]);
    }
  }

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
  write(fd, buffer, offset);
}

int kvs_backup() {
  return 0;
}

void kvs_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}