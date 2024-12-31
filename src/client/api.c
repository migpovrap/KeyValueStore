#include <string.h>

#include "api.h"

/// Sends a message to the KVS server or the request pipe.
/// @param opcode The operation code specifying the action to be performed.
/// @param client_data Pointer to a struct holding client-specific information.
/// @param key Optional key associated with the operation
/// (used for subscribe/unsubscribe); use NULL if not applicable.
/// @param registration_fifo_fd Pointer to the file descriptor for the
/// registration's FIFO; pass -1 if not applicable.
/// @return 0 if the message was sent successfully, 1 otherwise.
static int send_message(enum OperationCode opcode, const ClientData*
client_data, const char* key, const int* registration_fifo_fd) {
  char message[MAX_PIPE_PATH_LENGTH * 3 + 3 + sizeof(int)];
  switch (opcode) {
    case OP_CODE_CONNECT:
      // Send message to the registration pipe.
      snprintf(message, sizeof(message), "%d|%s|%s|%s", opcode,
      client_data->req_pipe_path, client_data->resp_pipe_path,
      client_data->notif_pipe_path);
      if (write(*registration_fifo_fd, message, strlen(message)) == -1) {
        fprintf(stderr, "Failed to write to the registration FIFO.\n");
        return 1;
      }
      return 0;
    case OP_CODE_DISCONNECT:
      snprintf(message, sizeof(message), "%d", opcode);
      break;
    case OP_CODE_SUBSCRIBE:
    case OP_CODE_UNSUBSCRIBE:
      snprintf(message, sizeof(message), "%d|%s", opcode, key);
      break;
  }
  // Send message to the request pipe.
  if (write(client_data->req_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write to the request FIFO.\n");
    return 1;
  }
  return 0;
}

/// Reads and validates the server's response.
/// @param resp_fifo_fd Pointer to the file descriptor for the response FIFO.
/// @return The server response code. 0 on success, or an error code otherwise.
static int check_server_response(const int* resp_fifo_fd) {
  char server_response[SERVER_RESPONSE_SIZE];
  if (read(*resp_fifo_fd, server_response, SERVER_RESPONSE_SIZE)
  != SERVER_RESPONSE_SIZE) {
    fprintf(stderr, "Failed to read the server response.\n");
  } else if (server_response[1] != 0) {
    fprintf(stderr, "Server responded with an error.\n");
  }
  return server_response[1];
}

int kvs_connect(ClientData* client_data, const char* registration_pipe_path) {
  int registration_fifo_fd = open(registration_pipe_path, O_WRONLY);

  if (registration_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the registration FIFO.\n");
    return 1;
  }

  if (send_message(OP_CODE_CONNECT, client_data, NULL, &registration_fifo_fd)) {
    close(registration_fifo_fd);
    return 1;
  }

  close(registration_fifo_fd);

  if (create_fifos() != 0) {
    fprintf(stderr, "Error creating FIFOs.\n");
    return 1;
  }
  
  client_data->req_fifo_fd = open(client_data->req_pipe_path, O_WRONLY);
  client_data->resp_fifo_fd = open(client_data->resp_pipe_path, O_RDONLY);
  client_data->notif_fifo_fd = open(client_data->notif_pipe_path,
  O_RDONLY | O_NONBLOCK);

  if (client_data->req_fifo_fd == -1 || client_data->resp_fifo_fd == -1 ||
  client_data->notif_fifo_fd == -1) {
    fprintf(stderr, "Failed to open FIFOs.\n");
    return 1;
  }

  int server_response = check_server_response(&client_data->resp_fifo_fd);
  printf("Server returned %d for operation: connect.\n", server_response);

  if (server_response != 0)
    return 1;

  return 0;
}

int kvs_disconnect(ClientData* client_data) {
  if (send_message(OP_CODE_DISCONNECT, client_data, NULL, NULL))
    return 1;

  int server_response = check_server_response(&client_data->resp_fifo_fd);
  printf("Server returned %d for operation: disconnect.\n", server_response);

  if (server_response != 0)
    return 1;
  return 0;
}

int kvs_subscribe(ClientData* client_data, const char* key) {
  // Check if max number of subscriptions has been reached.
  if (client_data->client_subs >= MAX_NUMBER_SUB) {
    fprintf(stderr, "Max number of subscriptions reached. \
    Please unsubscribe from a key before subscribing to another.\n");
    return 0;
  }

  if (send_message(OP_CODE_SUBSCRIBE, client_data, key, NULL)) {
    return 1;
  }

  int server_response = check_server_response(&client_data->resp_fifo_fd);
  printf("Server returned %d for operation: subscribe.\n", server_response);
  
  if (server_response != 0)
    return 1;

  // Increment client subscriptions.
  __sync_fetch_and_add(&client_data->client_subs, 1);
  return 0;
}

int kvs_unsubscribe(ClientData* client_data, const char* key) {
  if (send_message(OP_CODE_UNSUBSCRIBE, client_data, key, NULL))
    return 1;
  
  int server_response = check_server_response(&client_data->resp_fifo_fd);
  printf("Server returned %d for operation: unsubscribe.\n", server_response);

  if (server_response != 0)
    return 1;

  // Decrement client subscriptions.
  __sync_fetch_and_sub(&client_data->client_subs, 1);
  return 0;
}
