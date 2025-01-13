#ifndef SUBSCRIPTIONS_H
#define SUBSCRIPTIONS_H

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/constants.h"

typedef struct SubscriptionData {
  char key[MAX_STRING_SIZE];
  int notification_fifo_fd;
  struct SubscriptionData* next;
} SubscriptionData;

typedef struct ClientSubscriptions {
  pthread_mutex_t mutex;
  SubscriptionData* subscription_data;
} ClientSubscriptions;

#endif // SUBSCRIPTIONS_H