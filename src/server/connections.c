#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <semaphore.h>

#include "connections.h"
#include "notifications.h"
#include "common/protocol.h"
#include "common/constants.h"

RequestBuffer session_buffer;

void initialize_session_buffer() {
  session_buffer.in = 0;
  session_buffer.out = 0;
  sem_init(&session_buffer.full, 0, 0);
  sem_init(&session_buffer.empty, 0, MAX_SESSION_COUNT);
  pthread_mutex_init(&session_buffer.buffer_lock, NULL);
}

void produce_request(ClientListenerData request) {
  sem_wait(&session_buffer.empty);
  pthread_mutex_lock(&session_buffer.buffer_lock);

  session_buffer.session_data[session_buffer.in] = request;
  session_buffer.in = (session_buffer.in + 1) % MAX_SESSION_COUNT;

  pthread_mutex_unlock(&session_buffer.buffer_lock);
  sem_post(&session_buffer.full);
}

void disconnect_all_clients() {
  pthread_mutex_lock(&session_buffer.buffer_lock);

  // Iterate through the buffer and set the terminate flag for all clients
  for (int i = 0; i < MAX_SESSION_COUNT; i++) {
    atomic_store(&session_buffer.session_data[i].terminate, 0);
  }

  // Clean up the buffer
  session_buffer.in = 0;
  session_buffer.out = 0;

  // Release all semaphores
  while (sem_trywait(&session_buffer.full) == 0) {
    sem_post(&session_buffer.empty);
  }

  pthread_mutex_unlock(&session_buffer.buffer_lock);
}

void client_handle_subscriptions(int resp_fifo_fd, int notif_fifo_fd, char* key, enum OperationCode op_code) {
  char response[SERVER_RESPONSE_SIZE] = {op_code, 0};
  if (key != NULL) {
    if (op_code == OP_CODE_SUBSCRIBE)
      add_subscription(key, notif_fifo_fd);
    else if (op_code == OP_CODE_UNSUBSCRIBE)
      remove_subscription(key);
  } else {
    response[1] = 1; // An error ocurred
  }
  if (write(resp_fifo_fd, response, SERVER_RESPONSE_SIZE) == -1) {
    fprintf(stderr, "Failed to write to the response fifo of the client.\n");
  }
}

void client_handle_disconnect(int resp_fifo_fd, int req_fifo_fd, int notif_fifo_fd) {
  remove_client(notif_fifo_fd);
  char response[SERVER_RESPONSE_SIZE] = {OP_CODE_DISCONNECT, 0};
  if (write(resp_fifo_fd, response, SERVER_RESPONSE_SIZE) == -1) {
    fprintf(stderr, "Failed to write to the response fifo of the client.\n");
  }

  close(req_fifo_fd);
  close(resp_fifo_fd);
  close(notif_fifo_fd);
}

void handle_client_request(ClientListenerData request) {
  int req_fifo_fd = open(request.req_pipe_path, O_RDONLY | O_NONBLOCK);
  int resp_fifo_fd = open(request.resp_pipe_path, O_WRONLY);
  int notif_fifo_fd = open(request.notif_pipe_path, O_WRONLY);

  if (req_fifo_fd == -1 || resp_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the client fifos.\n");
    return;
  }
  char response[SERVER_RESPONSE_SIZE] = {OP_CODE_CONNECT, 0}; // 0 indicates success
  if (write(resp_fifo_fd, response, SERVER_RESPONSE_SIZE) == -1) {
    fprintf(stderr, "Failed to write to the response fifo of the client.\n");
  }
  char buffer[MAX_STRING_SIZE];
  while (atomic_load(&request.terminate)) {
    ssize_t bytes_read = read(req_fifo_fd, buffer, MAX_STRING_SIZE);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      char* token = strtok(buffer, "|");
      char* key;
      int op_code_int = atoi(token);
      enum OperationCode op_code = (enum OperationCode)op_code_int;

      switch (op_code) {
        case OP_CODE_SUBSCRIBE:
          key = strtok(NULL, "|");
          client_handle_subscriptions(resp_fifo_fd, notif_fifo_fd, key, OP_CODE_SUBSCRIBE);
          break;
        case OP_CODE_UNSUBSCRIBE:
          key = strtok(NULL, "|");
          client_handle_subscriptions(resp_fifo_fd, -1, key, OP_CODE_UNSUBSCRIBE);
          break;
        case OP_CODE_DISCONNECT:
          client_handle_disconnect(resp_fifo_fd, req_fifo_fd, notif_fifo_fd);
          return;
        case OP_CODE_CONNECT:
          // This case is not sent to request pipe only registry pipe (not read here).
          break;
        default:
          fprintf(stderr, "Unknown operation code: %d\n", op_code);
          response[0] = op_code;
          response[1] = 1; // Error unknown operation code
          if (write(resp_fifo_fd, response, 2) == -1) {
            fprintf(stderr, "Failed to write to the response fifo of the client.\n");
          }
          break;
      }
    }
  }
  close(req_fifo_fd);
  close(resp_fifo_fd);
  close(notif_fifo_fd);
}

void* client_request_handler() {
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

  while (1) {
    sem_wait(&session_buffer.full);
    pthread_mutex_lock(&session_buffer.buffer_lock);

    ClientListenerData request = session_buffer.session_data[session_buffer.out];
    session_buffer.out = (session_buffer.out + 1) % MAX_SESSION_COUNT;

    pthread_mutex_unlock(&session_buffer.buffer_lock);
    sem_post(&session_buffer.empty);

    handle_client_request(request);
  }
  return NULL;
}

void* connection_listener(void* args) {
  extern atomic_bool connection_listener_alive;
  char* server_pipe_path = (char*)args;
  int server_fifo_fd;
  
  // Creates a named pipe (FIFO)
  if(mkfifo(server_pipe_path, 0666) == -1 && errno != EEXIST) { // Used to not recreate the pipe, overwrite it. When multiplie listener threads exist.
    fprintf(stderr, "Failed to create the named pipe or one already exists with the same name.\n");
    pthread_exit(NULL);
  }

  // Opens the named pipe (FIFO) for reading
  if ((server_fifo_fd = open(server_pipe_path, O_RDONLY)) == -1) {
    fprintf(stderr, "Failed to open the named pipe (FIFO).\n");
    pthread_exit(NULL);
  }

  while (atomic_load(&connection_listener_alive)) {
    char buffer[MAX_STRING_SIZE];
    ssize_t bytes_read = read(server_fifo_fd, buffer, MAX_STRING_SIZE);
     if (bytes_read > 0) {
      buffer[bytes_read] = '\0';

      // Parse the client operation code request
      char* token = strtok(buffer, "|");
      int op_code_int = atoi(token);
      enum OperationCode op_code = (enum OperationCode)op_code_int;

      if (op_code == OP_CODE_CONNECT) {
        char* req_pipe_path = strtok(NULL, "|");
        char* resp_pipe_path = strtok(NULL, "|");
        char* notif_pipe_path = strtok(NULL, "|");

        ClientListenerData* listener_data = malloc(sizeof(ClientListenerData));
        if (listener_data == NULL) {
          fprintf(stderr, "Failed to allocate memory for ClientListenerData.\n");
          continue;
        }
        listener_data->req_pipe_path = req_pipe_path;
        listener_data->resp_pipe_path = resp_pipe_path;
        listener_data->notif_pipe_path = notif_pipe_path;
        atomic_store(&listener_data->terminate, 1);
        produce_request(*listener_data);

      }
    }
  }
  close(server_fifo_fd);
  unlink(server_pipe_path);
  clear_all_subscriptions();
  disconnect_all_clients();
  pthread_exit(NULL);
}
