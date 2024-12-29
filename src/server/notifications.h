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

/**
 * @brief Adds a subscription for a given key.
 *
 * This function adds a subscription for the specified key, associating it with
 * the provided notification FIFO file descriptor.
 *
 * @param key The key to subscribe to.
 * @param notification_fifo_fd The file descriptor for the notification FIFO.
 */
void add_subscription(const char* key, int notification_fifo_fd);

/**
 * @brief Removes a subscription for a given key.
 *
 * This function removes the subscription associated with the specified key.
 *
 * @param key The key to unsubscribe from.
 */
void remove_subscription(const char* key);

/**
 * @brief Removes a client from all subscriptions.
 *
 * This function removes the client associated with the specified notification
 * FIFO file descriptor from all subscriptions.
 *
 * @param notification_fifo_fd The file descriptor for the notification FIFO.
 */
void remove_client(int notification_fifo_fd);

/**
 * @brief Notifies all subscribers of a key with a new value.
 *
 * This function sends a notification to all subscribers of the specified key,
 * providing them with the new value.
 *
 * @param key The key whose subscribers will be notified.
 * @param value The new value to notify the subscribers with.
 */
void notify_subscribers(const char* key, const char* value);

/**
 * @brief Clears all subscriptions.
 *
 * This function removes all existing subscriptions.
 */
void clear_all_subscriptions();

#endif