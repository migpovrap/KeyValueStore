#include "notifications.h"
#include "operations.h"

extern ServerData* server_data;

int is_already_subscribed(const char* key, int notification_fifo_fd) {
  SubscriptionData* current = server_data->all_subscriptions.subscription_data;
  while (current != NULL) {
    if (strncmp(current->key, key, MAX_STRING_SIZE) == 0 && current->notification_fifo_fd == notification_fifo_fd) {
      return 1; // Subscription already exists.
    }
    current = current->next;
  }
  return 0; // Subscription does not exist.
}

int add_subscription(const char* key, int notification_fifo_fd) {
  // Check if the key exists in the KVS.
  if (key_exists(key)) {
    return 1; // Key does not exist.
  }

  if (is_already_subscribed(key, notification_fifo_fd)) {
    return 3;
  }

  SubscriptionData* new_sub = malloc(sizeof(SubscriptionData));
  if (new_sub == NULL) {
    write_str(STDERR_FILENO, "Failed to allocate memory for subscrpition data.\n");
    return 1;
  }

  strncpy(new_sub->key, key, MAX_STRING_SIZE);
  new_sub->notification_fifo_fd = notification_fifo_fd;
  new_sub->next = NULL;

  pthread_mutex_lock(&server_data->all_subscriptions.mutex);
  new_sub->next = server_data->all_subscriptions.subscription_data;
  server_data->all_subscriptions.subscription_data = new_sub;
  pthread_mutex_unlock(&server_data->all_subscriptions.mutex);

  return 0;
}

int remove_subscription(const char* key) {
  pthread_mutex_lock(&server_data->all_subscriptions.mutex);
  SubscriptionData* prev = NULL;
  SubscriptionData* sub_data =
  server_data->all_subscriptions.subscription_data;

  while (sub_data != NULL) {
    if (strncmp(sub_data->key, key, MAX_STRING_SIZE) == 0) {
      if (prev == NULL) {
        server_data->all_subscriptions.subscription_data = sub_data->next;
      } else {
        prev->next = sub_data->next;
      }
      free(sub_data);
      pthread_mutex_unlock(&server_data->all_subscriptions.mutex);
      return 0;
    }
    prev = sub_data;
    sub_data = sub_data->next;
  }

  pthread_mutex_unlock(&server_data->all_subscriptions.mutex);
  return 1; // Key not found in subscriptions.
}

void remove_client(int notification_fifo_fd) {
  pthread_mutex_lock(&server_data->all_subscriptions.mutex);

  SubscriptionData* prev = NULL;
  SubscriptionData* curr = server_data->all_subscriptions.subscription_data;

  while (curr != NULL) {
    if (curr->notification_fifo_fd == notification_fifo_fd) {
      if (prev == NULL) {
        server_data->all_subscriptions.subscription_data = curr->next;
      } else {
        prev->next = curr->next;
      }
      free(curr);
      curr = (prev == NULL) ?
      server_data->all_subscriptions.subscription_data : prev->next;
    } else {
      prev = curr;
      curr = curr->next;
    }
  }

  pthread_mutex_unlock(&server_data->all_subscriptions.mutex);
}

void notify_subscribers(const char* key, const char* value) {
  pthread_mutex_lock(&server_data->all_subscriptions.mutex);

  SubscriptionData* sub = server_data->all_subscriptions.subscription_data;
  while (sub != NULL) {
    if (strncmp(sub->key, key, MAX_STRING_SIZE) == 0) {
      char notification[MAX_STRING_SIZE * 2];
      snprintf(notification, sizeof(notification), "(%s,%s)", key, value);
      write(sub->notification_fifo_fd, notification, strlen(notification) + 1);
    }
    sub = sub->next;
  }
  pthread_mutex_unlock(&server_data->all_subscriptions.mutex);
}

void clear_all_subscriptions() {
  pthread_mutex_lock(&server_data->all_subscriptions.mutex);

  SubscriptionData* curr = server_data->all_subscriptions.subscription_data;
  while (curr != NULL) {
    SubscriptionData* temp = curr;
    curr = curr->next;
    free(temp);
  }
  server_data->all_subscriptions.subscription_data = NULL;

  pthread_mutex_unlock(&server_data->all_subscriptions.mutex);
}
