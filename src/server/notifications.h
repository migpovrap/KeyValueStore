#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <pthread.h>
#include "common/constants.h"

typedef struct SubscriptionData {
  char key[MAX_STRING_SIZE];
  int notification_fifo_fd;
  struct SubscriptionData* next;
} SubscriptionData;

typedef struct ClientSubscriptions {
  pthread_mutex_t lock;
  struct SubscriptionData* subscription_data;
} ClientSubscriptions;

void add_subscription(const char* key, int notification_fifo_fd);
void remove_subscription(const char* key);
void remove_client(int notification_fifo_fd);
void notify_subscribers(const char* key, const char* value);
void clear_all_subscriptions();
void join_all_client_threads();

#endif