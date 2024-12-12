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
    pthread_rwlock_init(&ht->hash_lock[i], NULL);
  }
  pthread_rwlock_init(&ht->table_lock, NULL);
  return ht;
}

int write_pair(HashTable *ht, const char *key, const char *value) {
  int index = hash(key);
  KeyNode *key_node = ht->table[index];
  // Search for the key node
  while (key_node != NULL) {
    if (strcmp(key_node->key, key) == 0) {
      free(key_node->value);
      key_node->value = strdup(value);
      return 0;
    }
    key_node = key_node->next; // Move to the next node
  }

  // Key not found, create a new key node
  key_node = malloc(sizeof(KeyNode));
  key_node->key = strdup(key); // Allocate memory for the key
  key_node->value = strdup(value); // Allocate memory for the value
  key_node->next = ht->table[index]; // Link to existing nodes
  ht->table[index] = key_node; // Place new key node at the start of the list
  return 0;
}

char* read_pair(HashTable *ht, const char *key) {
  int index = hash(key);
  KeyNode *key_node = ht->table[index];
  char* value;

  while (key_node != NULL) {
    if (strcmp(key_node->key, key) == 0) {
      value = strdup(key_node->value);
      return value; // Return copy of the value if found
    }
    key_node = key_node->next; // Move to the next node
  }
  return NULL; // Key not found
}

int delete_pair(HashTable *ht, const char *key) {
  int index = hash(key);
  KeyNode *key_node = ht->table[index];
  KeyNode *prev_node = NULL;

  while (key_node != NULL) {
    if (strcmp(key_node->key, key) == 0) {
      // Key found; delete this node
      if (prev_node == NULL) {
        ht->table[index] = key_node->next; // Update the table to point to the next node
      } else {
        // Node to delete is not the first; bypass it
        prev_node->next = key_node->next; // Link the previous node to the next node
      }
      // Free the memory allocated for the key and value
      free(key_node->key);
      free(key_node->value);
      free(key_node); // Free the key node itself
      return 0; // Exit the function
    } else {
      prev_node = key_node; // Move prev_node to current node
      key_node = key_node->next; // Move to the next node
    }
  }
  return 1;
}

void free_table(HashTable *ht) {
  pthread_rwlock_rdlock(&ht->table_lock);
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *key_node = ht->table[i];
    while (key_node != NULL) {
      KeyNode *temp = key_node;
      key_node = key_node->next;
      free(temp->key);
      free(temp->value);
      free(temp);
    }
    pthread_rwlock_destroy(&ht->hash_lock[i]);
  }
  pthread_rwlock_unlock(&ht->table_lock);
  pthread_rwlock_destroy(&ht->table_lock);
  free(ht);
}
