#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/constants.h"
#include "server/io.h"
#include "server/subscriptions.h"

/// Adds a subscription for a given key.
/// @param key The key to subscribe to.
/// @param notification_fifo_fd The file descriptor for the notification FIFO.
int add_subscription(const char* key, int notification_fifo_fd);

/// Removes a subscription for a given key.
/// @param key The key to unsubscribe from.
int remove_subscription(const char* key);

/// Removes a client from all subscriptions.
/// @param notification_fifo_fd The file descriptor for the notification FIFO.
void remove_client(int notification_fifo_fd);

/// Notifies all subscribers of a key with a new value.
/// @param key The key whose subscribers will be notified.
/// @param value The new value to notify the subscribers with.
void notify_subscribers(const char* key, const char* value);

/// Clears all subscriptions.
void clear_all_subscriptions();

#endif // NOTIFICATIONS_H
