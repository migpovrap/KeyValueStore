#include "kvs.h"

// Hash function based on key initial.
// @param key Lowercase alphabetical string.
// @return hash.
// NOTE: This is not an ideal hash function, but is useful for test purposes of the project
int hash(const char *key) {
  int first_letter = tolower(key[0]);
  if (first_letter >= 'a' && first_letter <= 'z') {
    return first_letter - 'a';
  } else if (first_letter >= '0' && first_letter <= '9') {
    return first_letter - '0';
  }
  return -1; // Invalid index for non-alphabetic or number strings
}

struct HashTable* create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht) return NULL;
  for (int i = 0; i < TABLE_SIZE; i++) {
    ht->table[i] = NULL;
    pthread_mutex_init(&ht->hash_mutex[i], NULL);
  }
  pthread_mutex_init(&ht->kvs_mutex, NULL);
  return ht;
}

int write_pair(HashTable *ht, const char *key, const char *value) {
  int index = hash(key);
  KeyNode *key_node = ht->table[index];
  // Search for the key node
  while (key_node != NULL) {
    pthread_mutex_lock(&key_node->node_mutex);
    if (strcmp(key_node->key, key) == 0) {
      free(key_node->value);
      key_node->value = strdup(value);
      pthread_mutex_unlock(&key_node->node_mutex);
      return 0;
    } else {
      pthread_mutex_unlock(&key_node->node_mutex);
    }
    key_node = key_node->next; // Move to the next node
  }

  // Key not found, create a new key node
  key_node = malloc(sizeof(KeyNode));
  pthread_mutex_init(&key_node->node_mutex, NULL);
  key_node->key = strdup(key); // Allocate memory for the key
  key_node->value = strdup(value); // Allocate memory for the value
  pthread_mutex_lock(&ht->hash_mutex[index]);
  key_node->next = ht->table[index]; // Link to existing nodes
  ht->table[index] = key_node; // Place new key node at the start of the list
  pthread_mutex_unlock(&ht->hash_mutex[index]);
  return 0;
}

char* read_pair(HashTable *ht, const char *key) {
  int index = hash(key);
  KeyNode *key_node = ht->table[index];
  char* value;

  while (key_node != NULL) {
    pthread_mutex_lock(&key_node->node_mutex);
    if (strcmp(key_node->key, key) == 0) {
      value = strdup(key_node->value);
      pthread_mutex_unlock(&key_node->node_mutex);
      return value; // Return copy of the value if found
    }
    pthread_mutex_unlock(&key_node->node_mutex);
    key_node = key_node->next; // Move to the next node
  }
  return NULL; // Key not found
}

int delete_pair(HashTable *ht, const char *key) {
  int index = hash(key);
  KeyNode *key_node = ht->table[index];
  KeyNode *prev_node = NULL;

  while (key_node != NULL) {
    pthread_mutex_lock(&key_node->node_mutex);
    if (strcmp(key_node->key, key) == 0) {
      pthread_mutex_unlock(&key_node->node_mutex);
      // Key found; delete this node
      if (prev_node == NULL) {
        // Node to delete is the first node in the list
        pthread_mutex_lock(&ht->hash_mutex[index]);
        ht->table[index] = key_node->next; // Update the table to point to the next node
        pthread_mutex_unlock(&ht->hash_mutex[index]);
      } else {
        // Node to delete is not the first; bypass it
        prev_node->next = key_node->next; // Link the previous node to the next node
      }
      // Free the memory allocated for the key and value
      pthread_mutex_lock(&key_node->node_mutex);
      free(key_node->key);
      free(key_node->value);
      pthread_mutex_unlock(&key_node->node_mutex);
      pthread_mutex_destroy(&key_node->node_mutex);
      free(key_node); // Free the key node itself
      return 0; // Exit the function
    } else {
      pthread_mutex_unlock(&key_node->node_mutex);
      prev_node = key_node; // Move prev_node to current node
      key_node = key_node->next; // Move to the next node
    }
  }
  return 1;
}

void free_table(HashTable *ht) {
  pthread_mutex_lock(&ht->kvs_mutex);
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *key_node = ht->table[i];
    while (key_node != NULL) {
      KeyNode *temp = key_node;
      key_node = key_node->next;
      free(temp->key);
      free(temp->value);
      pthread_mutex_destroy(&temp->node_mutex);
      free(temp);
    }
    pthread_mutex_destroy(&ht->hash_mutex[i]);
  }
  pthread_mutex_unlock(&ht->kvs_mutex);
  pthread_mutex_destroy(&ht->kvs_mutex);
  free(ht);
}
