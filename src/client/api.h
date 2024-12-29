#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/constants.h"
#include "utils.h"


/// Connects to a KVS server.
/// @param client_data Pointer to a struct holding client-specific information.
/// @param server_pipe_path Path to the named pipe for the server.
/// @return 0 if the connection was established successfully, 1 otherwise.
int kvs_connect(ClientData* client_data, const char* server_pipe_path);

/// Disconnects from a KVS server.
/// @param client_data Pointer to a struct holding client-specific information.
/// @return 0 in case of success, 1 otherwise.
int kvs_disconnect(ClientData* client_data);

/// Requests a subscription for a key.
/// @param client_data Pointer to a struct holding client-specific information.
/// @param key Key to be subscribed.
/// @return 0 if the key was subscribed successfully, 1 otherwise.
int kvs_subscribe(ClientData* client_data, const char* key);

/// Removes a subscription for a key.
/// @param client_data Pointer to a struct holding client-specific information.
/// @param key Key to be unsubscribed.
/// @return 0 if the key was unsubscribed successfully, 1 otherwise.
int kvs_unsubscribe(ClientData* client_data, const char* key);

#endif  // CLIENT_API_H
