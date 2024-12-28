#include "api.h"
#include "common/constants.h"
#include "common/protocol.h"

static int check_server_response(int response_fifo_fd, int* response_code) {
  char server_response[SERVER_RESPONSE_SIZE];
  if (read(response_fifo_fd, server_response, SERVER_RESPONSE_SIZE) != SERVER_RESPONSE_SIZE) {
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

static int send_subscribe_unsubscribe_message(int request_fifo_fd, enum OperationCode opcode, const char* key) {
  char message[MAX_STRING_SIZE];
  snprintf(message, MAX_STRING_SIZE, "%d|%s", opcode, key);

  // Send the message to the request pipe
  if (write(request_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    return 1;
  }
  return 0;
}

int kvs_connect(const char* req_pipe_path, const char* resp_pipe_path, const char* notif_pipe_path, const char* registry_fifo, int* request_fifo_fd, int* response_fifo_fd) {
  int registry_fifo_fd = open(registry_fifo, O_WRONLY);

  if (registry_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the registry named pipe (FIFO).\n");
    return 1;
  }

  char message[MAX_STRING_SIZE];
  snprintf(message, MAX_STRING_SIZE, "%d|%s|%s|%s", OP_CODE_CONNECT, req_pipe_path, resp_pipe_path, notif_pipe_path);
  
  // Send the subscribe message to the request pipe
  ssize_t write_success = write(registry_fifo_fd, message, strlen(message));
  // Either way (whether the message was written successfully or unsuccessfully) close the registry FIFO.
  close(registry_fifo_fd);

  if (write_success == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    return 1;
  }

  // Opens the named pipes (FIFOS) with the correct permissions
  *request_fifo_fd = open(req_pipe_path, O_WRONLY);
  *response_fifo_fd = open(resp_pipe_path, O_RDONLY);

  if (*request_fifo_fd == -1 || *response_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the named pipes (FIFOs).\n");
    if (*request_fifo_fd != -1) close(*request_fifo_fd);
    if (*response_fifo_fd != -1) close(*response_fifo_fd);
    return 1;
  }

  int response_code;
  // Verify connection was successful
  if (check_server_response(*response_fifo_fd, &response_code)) {
    close(*request_fifo_fd);
    close(*response_fifo_fd);
    return 1;
  }
  
  // Print the server response
  printf("Server returned %d for operation: connect.\n", response_code);
  return 0;
}
 
int kvs_disconnect(int* request_fifo_fd, int* response_fifo_fd) {
  char message[2];
  snprintf(message, sizeof(message), "%d", OP_CODE_DISCONNECT);

  int status = 0;

  // Send the disconnect message to the request pipe.
  if (write(*request_fifo_fd, message, sizeof(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    status = 1;
  } else {
    int response_code;
    // If the disconnect message to the request pipe is sent successfully verify server response.
    status = check_server_response(*response_fifo_fd, &response_code);  
    // Print the server response
    printf("Server returned %d for operation: disconnect.\n", response_code);
  }
  
  // Close the named pipes
  close(*request_fifo_fd);
  close(*response_fifo_fd);

  return status;
}

int kvs_subscribe(int* request_fifo_fd, int* response_fifo_fd, const char* key) {
  // Send the subscribe message to the request pipe
  if (send_subscribe_unsubscribe_message(*request_fifo_fd, OP_CODE_SUBSCRIBE, key)) {
    return 1;
  }

  int response_code;
  // Verify server response
  if (check_server_response(*response_fifo_fd, &response_code)) {
    return 1;
  }

  // Print the server response
  printf("Server returned %d for operation: subscribe.\n", response_code);
  return 0;
}

int kvs_unsubscribe(int* request_fifo_fd, int* response_fifo_fd, const char* key) {
  // Send the unsubscribe message to the request pipe
  if (send_subscribe_unsubscribe_message(*request_fifo_fd, OP_CODE_UNSUBSCRIBE, key)) {
    return 1;
  }
  
  int response_code;
  // Verify server response
  if (check_server_response(*response_fifo_fd, &response_code)) {
    return 1;
  }

  // Print the server response
  printf("Server returned %d for operation: unsubscribe.\n", response_code);
  return 0;
}
