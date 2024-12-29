#include "api.h"
#include "common/constants.h"
#include "common/protocol.h"

static int check_server_response(int resp_fifo_fd, int* response_code) {
  char server_response[SERVER_RESPONSE_SIZE];
  if (read(resp_fifo_fd, server_response, SERVER_RESPONSE_SIZE) != SERVER_RESPONSE_SIZE) {
    fprintf(stderr, "Failed to read the server response.\n");
    return 1;
  }
  *response_code = server_response[1];
  if (*response_code != 0) {
    fprintf(stderr, "Server responded with an error.\n");
    return 1;
  }
  return 0;
}

static int send_request_message(int req_fifo_fd, enum OperationCode opcode, const char* key) {
  char message[sizeof(int) + MAX_STRING_SIZE];
  snprintf(message, sizeof(int) + MAX_STRING_SIZE, "%d|%s", opcode, key);

  // Send the message to the request pipe
  if (write(req_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write to the request FIFO.\n");
    return 1;
  }
  return 0;
}

int kvs_connect(ClientData* client_data, const char* server_pipe_path) {
  int server_fifo_fd = open(server_pipe_path, O_WRONLY);

  if (server_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the server FIFO.\n");
    return 1;
  }

  char message[sizeof(int) + MAX_PIPE_PATH_LENGTH * 3 + 3];
  snprintf(message, sizeof(int) + MAX_PIPE_PATH_LENGTH * 3 + 3, "%d|%s|%s|%s", OP_CODE_CONNECT, client_data->req_pipe_path, client_data->resp_pipe_path, client_data->notif_pipe_path);
  
  // Send the subscribe message to the request pipe
  ssize_t write_success = write(server_fifo_fd, message, strlen(message));
  // Either way (whether the message was written successfully or unsuccessfully) close the server FIFO.
  close(server_fifo_fd);

  if (write_success == -1) {
    fprintf(stderr, "Failed to write to the server FIFO.\n");
    return 1;
  }

  // Opens the named pipes (FIFOS) with the correct permissions

  client_data->req_fifo_fd = open(client_data->req_pipe_path, O_WRONLY);
  client_data->resp_fifo_fd = open(client_data->resp_pipe_path, O_RDONLY);
  client_data->notif_fifo_fd = open(client_data->notif_pipe_path, O_RDONLY | O_NONBLOCK);

  if (client_data->req_fifo_fd == -1 || client_data->resp_fifo_fd == -1 || client_data->notif_fifo_fd == -1) {
    fprintf(stderr, "Failed to open FIFOs.\n");
    if (client_data->req_fifo_fd != -1) close(client_data->req_fifo_fd);
    if (client_data->resp_fifo_fd != -1) close(client_data->resp_fifo_fd);
    if (client_data->notif_fifo_fd != -1) close(client_data->notif_fifo_fd);
    return 1;
  }

  int response_code;
  // Verify connection was successful
  if (check_server_response(client_data->resp_fifo_fd, &response_code)) {
    close(client_data->req_fifo_fd);
    close(client_data->resp_fifo_fd);
    close(client_data->notif_fifo_fd);
    return 1;
  }
  
  // Print the server response
  printf("Server returned %d for operation: connect.\n", response_code);
  return 0;
}
 
int kvs_disconnect(ClientData* client_data) {
  char message[2];
  snprintf(message, sizeof(message), "%d", OP_CODE_DISCONNECT);

  int status = 0;

  // Send the disconnect message to the request pipe.
  if (write(client_data->req_fifo_fd, message, sizeof(message)) == -1) {
    fprintf(stderr, "Failed to write to the request FIFO.\n");
    status = 1;
  } else {
    int response_code;
    // If the disconnect message to the request pipe is sent successfully verify server response.
    status = check_server_response(client_data->resp_fifo_fd, &response_code);  
    // Print the server response
    printf("Server returned %d for operation: disconnect.\n", response_code);
  }
  
  // Close the named pipes
  close(client_data->req_fifo_fd);
  close(client_data->resp_fifo_fd);
  return status;
}

int kvs_subscribe(ClientData* client_data, const char* key) {
  // TODO: Use mutex pass as argument.
  if (client_data->client_subs >= MAX_NUMBER_SUB) {
    printf("Max number of subscriptions reached please unsubscribe from a key before subscribing to another.\n");
    return 0;
  }

  if (send_request_message(client_data->req_fifo_fd, OP_CODE_SUBSCRIBE, key)) {
    return 1;
  }

  int response_code;
  if (check_server_response(client_data->resp_fifo_fd, &response_code)) {
    return 1;
  }

  __sync_fetch_and_add(&client_data->client_subs, 1);

  printf("Server returned %d for operation: subscribe.\n", response_code);
  return 0;
}

int kvs_unsubscribe(ClientData* client_data, const char* key) {
  if (send_request_message(client_data->req_fifo_fd, OP_CODE_UNSUBSCRIBE, key)) {
    return 1;
  }
  
  int response_code;
  if (check_server_response(client_data->resp_fifo_fd, &response_code)) {
    return 1;
  }

  __sync_fetch_and_sub(&client_data->client_subs, 1);

  printf("Server returned %d for operation: unsubscribe.\n", response_code);
  return 0;
}
