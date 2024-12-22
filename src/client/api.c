#include "api.h"
#include "common/constants.h"
#include "common/protocol.h"

int kvs_connect(const char* req_pipe_path, const char* resp_pipe_path, const char* notif_pipe_path, const char* registry_fifo) {
  int regestry_fifo_fd = open(registry_fifo, O_WRONLY);

  if (regestry_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the regestry named pipe (FIFO).\n");
    return 1;
  }

  char message[MAX_STRING_SIZE];
  snprintf(message, MAX_STRING_SIZE, "%d|%s|%s|%s", OP_CODE_CONNECT, req_pipe_path, resp_pipe_path, notif_pipe_path);
  
  if (write(regestry_fifo_fd, message, strlen(message)) == -1) {
    fprintf(stderr, "Failed to write on the registry named pipe (FIFO).\n");
    close(regestry_fifo_fd);
    return 1;
  }

   // Opens all the named pipes (FIFOS) with the correct permissions
  int request_fifo_fd = open(req_pipe_path, O_WRONLY);
  int response_fifo_fd = open(resp_pipe_path, O_RDONLY);
  int notification_fifo_fd = open(notif_pipe_path, O_RDONLY);

  if (request_fifo_fd == -1 || response_fifo_fd == -1 || notification_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the named pipes (FIFOs).\n");
    return 1;
  }
  // Verify connection was successful
  char server_response[2];
  if (read(response_fifo_fd, server_response, 2) != 2) {
    fprintf(stderr, "Failed to read the server response.\n");
    close(regestry_fifo_fd);
    close(request_fifo_fd);
    close(response_fifo_fd);
    close(notification_fifo_fd);
    return 1;
  }

  if (server_response[1] != 0) {
    fprintf(stderr, "Sever responded with an error.\n");
    close(regestry_fifo_fd);
    close(request_fifo_fd);
    close(response_fifo_fd);
    close(notification_fifo_fd);
    return 1;
  }

  // Print the server response
  printf("Server returned %d for operation: connect\n", server_response[1]);

  return 0;
}
 
int kvs_disconnect(void) {
  // close pipes and unlink pipe files
  return 0;
}

int kvs_subscribe(const char* key) {
  // send subscribe message to request pipe and wait for response in response pipe
  return 0;
}

int kvs_unsubscribe(const char* key) {
    // send unsubscribe message to request pipe and wait for response in response pipe
  return 0;
}


