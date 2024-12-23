#include "api.h"
#include "common/constants.h"
#include "common/protocol.h"


int kvs_connect(const char* req_pipe_path, const char* resp_pipe_path, const char* notif_pipe_path, const char* registry_fifo, int* request_fifo_fd, int* response_fifo_fd) {
  int registry_fifo_fd = open(registry_fifo, O_WRONLY);

  if (registry_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the regestry named pipe (FIFO).\n");
    return 1;
  }

  char message[MAX_STRING_SIZE];
  snprintf(message, MAX_STRING_SIZE, "%d|%s|%s|%s", OP_CODE_CONNECT, req_pipe_path, resp_pipe_path, notif_pipe_path);
  
  // Send the subscribe message to the request pipe
  if (write(registry_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    close(registry_fifo_fd);
    return 1;
  }

   // Opens the named pipes (FIFOS) with the correct permissions
  *request_fifo_fd = open(req_pipe_path, O_WRONLY);
  *response_fifo_fd = open(resp_pipe_path, O_RDONLY);

  if (*request_fifo_fd == -1 || *response_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the named pipes (FIFOs).\n");
    return 1;
  }
  // Verify connection was successful
  char server_response[2];
  if (read(*response_fifo_fd, server_response, 2) != 2) {
    fprintf(stderr, "Failed to read the server response.\n");
    close(registry_fifo_fd);
    close(*request_fifo_fd);
    close(*response_fifo_fd);
    return 1;
  }

  if (server_response[1] != 0) {
    fprintf(stderr, "Sever responded with an error.\n");
    close(registry_fifo_fd);
    close(*request_fifo_fd);
    close(*response_fifo_fd);
    return 1;
  }

  // Print the server response
  printf("Server returned %d for operation: connect.\n", server_response[1]);
  close(registry_fifo_fd);
  return 0;
}
 
int kvs_disconnect(int* request_fifo_fd, int* response_fifo_fd) {

  char message[2];
  snprintf(message, sizeof(message), "%d", OP_CODE_DISCONNECT);

  // Send the subscribe message to the request pipe
  if (write(*request_fifo_fd, message, sizeof(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    return 1;
  }

  // Verify server response
  char server_response[2];
  if (read(*response_fifo_fd, server_response, 2) != 2) {
    fprintf(stderr, "Failed to read the server response.\n");
    close(*request_fifo_fd);
    close(*response_fifo_fd);
    return 1;
  }

  if (server_response[1] != 0) {
    fprintf(stderr, "Sever responded with an error.\n");
    close(*request_fifo_fd);
    close(*response_fifo_fd);
    return 1;
  }
  // Print the server response
  printf("Server returned %d for operation: disconnect.\n", server_response[1]);
  // Close the named pipes
  close(*request_fifo_fd);
  close(*response_fifo_fd);
  return 0;
}

int kvs_subscribe(int* request_fifo_fd, int* response_fifo_fd, const char* key) {

  char message[MAX_STRING_SIZE];
  snprintf(message, MAX_STRING_SIZE, "%d|%s", OP_CODE_SUBSCRIBE, key);

  // Send the subscribe message to the request pipe
  if (write(*request_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    return 1;
  }

  // Verify server response
  char server_response[2];
  if (read(*response_fifo_fd, server_response, 2) != 2) {
    fprintf(stderr, "Failed to read the server response.\n");
    return 1;
  }

  if (server_response[1] != 0) {
    fprintf(stderr, "Sever responded with an error.\n");
    return 1;
  }
  // Print the server response
  printf("Server returned %d for operation: subscribe.\n", server_response[1]);
  return 0;
}

int kvs_unsubscribe(int* request_fifo_fd, int* response_fifo_fd, const char* key) {

  char message[MAX_STRING_SIZE];
  snprintf(message, MAX_STRING_SIZE, "%d|%s", OP_CODE_UNSUBSCRIBE, key);

  // Send the unsubscribe message to the request pipe
  if (write(*request_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    return 1;
  }

  // Verify server response
  char server_response[2];
  if (read(*response_fifo_fd, server_response, 2) != 2) {
    fprintf(stderr, "Failed to read the server response.\n");
    return 1;
  }

  if (server_response[1] != 0) {
    fprintf(stderr, "Sever responded with an error.\n");
    return 1;
  }
  // Print the server response
  printf("Server returned %d for operation: unsubscribe.\n", server_response[1]);
  return 0;
}
