#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "common/constants.h"
#include "common/protocol.h"
#include "connections.h"
#include "notifications.h"
#include "server/io.h"
#include "server/utils.h"

extern ServerData* server_data;
RequestBuffer session_buffer;

void initialize_session_buffer() {
  session_buffer.in = 0;
  session_buffer.out = 0;
  session_buffer.session_data = malloc(MAX_SESSION_COUNT * sizeof(ClientData));
  if (session_buffer.session_data == NULL) {
    write_str(STDERR_FILENO, "Failed to allocate memory for session_data.\n");
    exit(1); // TODO REFACTOR ATEXIT LOGIC
  }
  sem_init(&session_buffer.full, 0, 0);
  sem_init(&session_buffer.empty, 0, MAX_SESSION_COUNT);
  pthread_mutex_init(&session_buffer.buffer_mutex, NULL);
}

void cleanup_session_buffer() {
  free(session_buffer.session_data);
  sem_destroy(&session_buffer.full);
  sem_destroy(&session_buffer.empty);
  pthread_mutex_destroy(&session_buffer.buffer_mutex);
}

void create_request(ClientData* client_data) {
  sem_wait(&session_buffer.empty);
  pthread_mutex_lock(&session_buffer.buffer_mutex);

  session_buffer.session_data[session_buffer.in] = *client_data;
  session_buffer.in = (session_buffer.in + 1) % MAX_SESSION_COUNT;
  
  pthread_mutex_unlock(&session_buffer.buffer_mutex);
  sem_post(&session_buffer.full);
}

void send_message (int resp_fifo_fd,
enum OperationCode op_code, int error_code) {
  char response[SERVER_RESPONSE_SIZE] = {op_code, (char) error_code};
  if (write(resp_fifo_fd, response, SERVER_RESPONSE_SIZE) == -1) {
    write_str(STDERR_FILENO,
    "Failed to write to the client's response FIFO.\n");
  }
}

void disconnect_all_clients() {
  pthread_mutex_lock(&session_buffer.buffer_mutex);

  // Iterate through the buffer and set the terminate flag for all clients.
  for (int i = 0; i < MAX_SESSION_COUNT; ++i)
    atomic_store(&session_buffer.session_data[i].terminate, 1);

  // Clean up the buffer.
  session_buffer.in = 0;
  session_buffer.out = 0;

  // Release all semaphores.
  while (sem_trywait(&session_buffer.full) == 0)
    sem_post(&session_buffer.empty);

  pthread_mutex_unlock(&session_buffer.buffer_mutex);
}

void handle_client_subscriptions(int resp_fifo_fd, int notif_fifo_fd,
char* key, enum OperationCode op_code) {
  int result = 0;
  if (key != NULL) {
    if (op_code == OP_CODE_SUBSCRIBE)
      result = add_subscription(key, notif_fifo_fd);
    else if (op_code == OP_CODE_UNSUBSCRIBE)
      result = remove_subscription(key);
  } else {
    result = 1; // If the key is NULL an error ocurred.
  }
  send_message(resp_fifo_fd, op_code, result);
}

void handle_client_disconnect(int resp_fifo_fd, int req_fifo_fd,
int notif_fifo_fd) {
  remove_client(notif_fifo_fd);

  send_message(resp_fifo_fd, OP_CODE_DISCONNECT, 0);
  
  close(req_fifo_fd);
  close(resp_fifo_fd);
  close(notif_fifo_fd);
}

void handle_client_request(ClientData* client_data) {
  int req_fifo_fd = open(client_data->req_pipe_path, O_RDONLY | O_NONBLOCK);
  int resp_fifo_fd = open(client_data->resp_pipe_path, O_WRONLY);
  int notif_fifo_fd = open(client_data->notif_pipe_path, O_WRONLY);

  if (req_fifo_fd == -1 || resp_fifo_fd == -1 || notif_fifo_fd == -1) {
    write_str(STDERR_FILENO, "Failed to open the client FIFOs.\n");
    return;
  }

  send_message(resp_fifo_fd, OP_CODE_CONNECT, 0);
  
  char buffer[MAX_STRING_SIZE];
  while (!atomic_load(&client_data->terminate)) {
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
          handle_client_subscriptions(resp_fifo_fd,
          notif_fifo_fd, key, OP_CODE_SUBSCRIBE);
          break;
        case OP_CODE_UNSUBSCRIBE:
          key = strtok(NULL, "|");
          handle_client_subscriptions(resp_fifo_fd, -1,
          key, OP_CODE_UNSUBSCRIBE);
          break;
        case OP_CODE_DISCONNECT:
          handle_client_disconnect(resp_fifo_fd, req_fifo_fd, notif_fifo_fd);
          return;
        case OP_CODE_CONNECT:
          // This case is not read here since it is sent to the server pipe.
          break;
        default:
          fprintf(stderr, "Unknown operation code: %d\n", op_code);
          send_message(resp_fifo_fd, op_code, 1);
          break;
      }
    }
  }
  close(req_fifo_fd);
  close(resp_fifo_fd);
  close(notif_fifo_fd);
}

// Worker Threads Function
void* client_request_handler() {
  // Blocks SIGUSR1 signal in the threads assigned to this function.
  sigset_t blocked_signals;
  sigemptyset(&blocked_signals);
  sigaddset(&blocked_signals, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &blocked_signals, NULL);
  
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

  while (1) {
    sem_wait(&session_buffer.full);
    pthread_mutex_lock(&session_buffer.buffer_mutex);

    ClientData* client_data =
    &session_buffer.session_data[session_buffer.out];
    session_buffer.out = (session_buffer.out + 1) % MAX_SESSION_COUNT;

    pthread_mutex_unlock(&session_buffer.buffer_mutex);
    sem_post(&session_buffer.empty);

    handle_client_request(client_data);
  }
  return NULL;
}

// Thread Manager Function
void* connection_manager(void* args) {
  char* server_pipe_path = (char*)args;
  
  // Creates a FIFO.
  if(mkfifo(server_pipe_path, 0666) == -1 && errno != EEXIST) {
    write_str(STDERR_FILENO, "Failed to create the named pipe or one \
    already exists with the same name.\n");
    pthread_exit(NULL);
  }

  // Opens FIFO for reading.
  int server_fifo_fd;
  if ((server_fifo_fd = open(server_pipe_path, O_RDONLY | O_NONBLOCK)) == -1) {
    write_str(STDERR_FILENO, "Failed to open FIFO.\n");
    pthread_exit(NULL);
  }

  while (!atomic_load(&server_data->terminate)) {
    // Check SIGUSR1.
    if (server_data->sigusr1_received) {
      clear_all_subscriptions();
      disconnect_all_clients();
      server_data->sigusr1_received = 0;
    }
    
    char buffer[MAX_STRING_SIZE];
    ssize_t bytes_read = read(server_fifo_fd, buffer, MAX_STRING_SIZE);
     if (bytes_read > 0) {
      buffer[bytes_read] = '\0';

      // Parse the client request operation code.
      char* token = strtok(buffer, "|");
      int op_code_int = atoi(token);
      enum OperationCode op_code = (enum OperationCode)op_code_int;

      if (op_code == OP_CODE_CONNECT) {
        char* req_pipe_path = strtok(NULL, "|");
        char* resp_pipe_path = strtok(NULL, "|");
        char* notif_pipe_path = strtok(NULL, "|");
        
        ClientData* client_data = malloc(sizeof(ClientData));
        client_data->req_pipe_path = strdup(req_pipe_path);
        client_data->resp_pipe_path = strdup(resp_pipe_path);
        client_data->notif_pipe_path = strdup(notif_pipe_path);
        atomic_store(&client_data->terminate, 0);
        create_request(client_data);

      }
    }
  }
  close(server_fifo_fd);
  unlink(server_pipe_path);
  clear_all_subscriptions();
  disconnect_all_clients();
  cleanup_session_buffer();
  pthread_exit(NULL);
}
