#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "notifications.h"
#include "operations.h"
#include "server/io.h"

int add_subscription(const char* key, int notification_fifo_fd) {
  extern ClientSubscriptions all_subscriptions;

  // Check if the key exists in the KVS.
  if (key_exists(key)) {
    return 1; // Key does not exist.
  }

  SubscriptionData* new_sub = malloc(sizeof(SubscriptionData));
  if (new_sub == NULL) {
    write_str(STDERR_FILENO, "Failed to allocate memory for\
    subscrpition data.\n");
    return 1;
  }

  strncpy(new_sub->key, key, MAX_STRING_SIZE);
  new_sub->notification_fifo_fd = notification_fifo_fd;
  new_sub->next = NULL;

  pthread_mutex_lock(&all_subscriptions.mutex);
  new_sub->next = all_subscriptions.subscription_data;
  all_subscriptions.subscription_data = new_sub;
  pthread_mutex_unlock(&all_subscriptions.mutex);

  return 0;
}

int remove_subscription(const char* key) {
  extern ClientSubscriptions all_subscriptions;

  pthread_mutex_lock(&all_subscriptions.mutex);
  SubscriptionData* prev = NULL;
  SubscriptionData* sub_data = all_subscriptions.subscription_data;

  while (sub_data != NULL) {
    if (strncmp(sub_data->key, key, MAX_STRING_SIZE) == 0) {
      if (prev == NULL) {
        all_subscriptions.subscription_data = sub_data->next;
      } else {
        prev->next = sub_data->next;
      }
      free(sub_data);
      pthread_mutex_unlock(&all_subscriptions.mutex);
      return 0;
    }
    prev = sub_data;
    sub_data = sub_data->next;
  }

  pthread_mutex_unlock(&all_subscriptions.mutex);
  return 1; // Key not found in subscriptions.
}

void remove_client(int notification_fifo_fd) {
  extern ClientSubscriptions all_subscriptions;

  pthread_mutex_lock(&all_subscriptions.mutex);

  SubscriptionData* prev = NULL;
  SubscriptionData* curr = all_subscriptions.subscription_data;

  while (curr != NULL) {
    if (curr->notification_fifo_fd == notification_fifo_fd) {
      if (prev == NULL) {
        all_subscriptions.subscription_data = curr->next;
      } else {
        prev->next = curr->next;
      }
      free(curr);
      curr = (prev == NULL) ? all_subscriptions.subscription_data : prev->next;
    } else {
      prev = curr;
      curr = curr->next;
    }
  }

  pthread_mutex_unlock(&all_subscriptions.mutex);
}

void notify_subscribers(const char* key, const char* value) {
  extern  ClientSubscriptions all_subscriptions;

  pthread_mutex_lock(&all_subscriptions.mutex);

  SubscriptionData* sub = all_subscriptions.subscription_data;
  while (sub != NULL) {
    if (strncmp(sub->key, key, MAX_STRING_SIZE) == 0) {
      char notification[MAX_STRING_SIZE * 2];
      snprintf(notification, sizeof(notification), "(%s,%s)", key, value);
      write(sub->notification_fifo_fd, notification, strlen(notification) + 1);
    }
    sub = sub->next;
  }
  pthread_mutex_unlock(&all_subscriptions.mutex);
}

void clear_all_subscriptions() {
  extern  ClientSubscriptions all_subscriptions;
  pthread_mutex_lock(&all_subscriptions.mutex);

  SubscriptionData* curr = all_subscriptions.subscription_data;
  while (curr != NULL) {
    SubscriptionData* temp = curr;
    curr = curr->next;
    free(temp);
  }
  all_subscriptions.subscription_data = NULL;

  pthread_mutex_unlock(&all_subscriptions.mutex);
  pthread_mutex_destroy(&all_subscriptions.mutex);
}
