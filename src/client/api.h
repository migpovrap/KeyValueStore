#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "common/constants.h"

#define SERVER_RESPONSE_SIZE 2

/// Connects to a kvs server.
/// @param req_pipe_path Path to the name pipe to be created for requests.
/// @param resp_pipe_path Path to the name pipe to be created for responses.
/// @param server_pipe_path Path to the name pipe where the server is listening.
/// @return 0 if the connection was established successfully, 1 otherwise.
int kvs_connect(const char* req_pipe_path, const char* resp_pipe_path, const char* notif_pipe_path, const char* registry_fifo, int* req_fifo_fd, int* resp_fifo_fd, int* notif_fifo_fd);

/// Disconnects from an KVS server.
/// @return 0 in case of success, 1 otherwise.
int kvs_disconnect(int* req_fifo_fd, int* resp_fifo_fd);

/// Requests a subscription for a key
/// @param key Key to be subscribed
/// @return 1 if the key was subscribed successfully (key existing), 0 otherwise.

int kvs_subscribe(int* req_fifo_fd, int* resp_fifo_fd, const char* key);

/// Remove a subscription for a key
/// @param key Key to be unsubscribed
/// @return 0 if the key was unsubscribed successfully  (subscription existed and was removed), 1 otherwise.

int kvs_unsubscribe(int* req_fifo_fd, int* resp_fifo_fd, const char* key);
 
#endif  // CLIENT_API_H
