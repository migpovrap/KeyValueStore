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

ClientThreads* client_threads = NULL;
pthread_mutex_t client_threads_lock = PTHREAD_MUTEX_INITIALIZER;

void add_client_thread(pthread_t new_thread, struct ClientFIFOs* client_data) {
  ClientThreads* new_client_thread = malloc(sizeof(ClientThreads));
  if (new_client_thread == NULL) {
    perror("malloc");
    return;
  }
  
  atomic_store(&new_client_thread->client_listener_alive, 1);
  new_client_thread->thread = new_thread;
  new_client_thread->next = NULL;

  pthread_mutex_lock(&client_threads_lock);
  new_client_thread->next = client_threads;
  client_threads = new_client_thread;
  pthread_mutex_unlock(&client_threads_lock);

  client_data->thread_data = new_client_thread;
}

void join_all_client_threads() {
  pthread_mutex_lock(&client_threads_lock);

  ClientThreads* curr = client_threads;
  while (curr != NULL) {
    atomic_store(&curr->client_listener_alive, 0); // Set the flag for thread exit
    curr = curr->next;
  }
  
  curr = client_threads;
  while (curr != NULL) {
    pthread_join(curr->thread, NULL);
    ClientThreads* temp = curr;
    curr = curr->next;
    free(temp);
  }

  client_threads = NULL;
  pthread_mutex_unlock(&client_threads_lock);
  pthread_mutex_destroy(&client_threads_lock);
}

void* client_request_listener(void* args) {
  extern sem_t max_clients;
  struct ClientFIFOs* client_data = (struct ClientFIFOs*)args;

  int req_fifo_fd = open(client_data->req_pipe_path, O_RDONLY);
  int resp_fifo_fd = open(client_data->resp_pipe_path, O_WRONLY);
  int notif_fifo_fd = open(client_data->notif_pipe_path, O_WRONLY);

  if (req_fifo_fd == -1 || resp_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the client fifos.\n");
    pthread_exit(NULL);
  } 

  // Send result op code to to client
  char response[2] = {OP_CODE_CONNECT, 0}; // 0 indicates success
  if (write(resp_fifo_fd, response, 2) == -1) {
    fprintf(stderr, "Failed to write to the response fifo of the client.\n");
  }

  char buffer[MAX_STRING_SIZE];

  while (atomic_load(&client_data->thread_data->client_listener_alive)) {
    ssize_t bytes_read = read(req_fifo_fd, buffer, MAX_STRING_SIZE);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';

      // Parse the client operation code request 
      char* token = strtok(buffer, "|");
      char* key;
      int op_code_int = atoi(token);
      enum OperationCode op_code = (enum OperationCode)op_code_int;

      switch (op_code) {
        case OP_CODE_SUBSCRIBE:
          key = strtok(NULL, "|");
          if (key != NULL) {
            add_subscription(key, notif_fifo_fd);
            response[0] = OP_CODE_SUBSCRIBE;
            response[1] = 0; // Success
          } else {
            response[0] = OP_CODE_SUBSCRIBE;
            response[1] = 1; // Error
          }
          if (write(resp_fifo_fd, response, 2) == -1) {
            fprintf(stderr, "Failed to write to the response fifo of the client.\n");
          }
          break;

        case OP_CODE_UNSUBSCRIBE:
          key = strtok(NULL, "|");
          if (key != NULL) {
            remove_subscription(key);
            response[0] = OP_CODE_UNSUBSCRIBE;
            response[1] = 0; // Success
          } else {
            response[0] = OP_CODE_UNSUBSCRIBE;
            response[1] = 1; // Error
          }
          if (write(resp_fifo_fd, response, 2) == -1) {
            fprintf(stderr, "Failed to write to the response fifo of the client.\n");
          }
          break;

        case OP_CODE_DISCONNECT:
          // Remove all subscriptions on the hashtable
          remove_client(notif_fifo_fd);
          // Send result op code to to client
          char disconnect_response[2] = {OP_CODE_DISCONNECT, 0}; // 0 indicates success
          if (write(resp_fifo_fd, disconnect_response, 2) == -1) {
            fprintf(stderr, "Failed to write to the response fifo of the client.\n");
          }
          // Closes the client fifos (open on the server)
          close(req_fifo_fd);
          close(resp_fifo_fd);
          close(notif_fifo_fd);
          free(client_data);
          sem_post(&max_clients);
          pthread_exit(NULL);
          break;
        case OP_CODE_CONNECT:
          // This case is not sent to request pipe only registry pipe (not read here).
          break;
        default:
          fprintf(stderr, "Unknown operation code: %d\n", op_code);
          response[0] = op_code;
          response[1] = 1; // Error unkown operation code
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
  free(client_data);
  sem_post(&max_clients);
  pthread_exit(NULL);
}

void* connection_listener(void* args) {
  extern atomic_bool connection_listener_alive;
  extern sem_t max_clients;
  char* registry_fifo_path = (char*)args;
  int registry_fifo_fd;
  
  // Creates a named pipe (FIFO)
  if(mkfifo(registry_fifo_path, 0666) == -1 && errno != EEXIST) { // Used to not recreate the pipe, overwrite it. When multiplie listener threads exist.
    fprintf(stderr, "Failed to create the named pipe or one already exists with the same name.\n");
    pthread_exit(NULL);
  }

  // Opens the named pipe (FIFO) for reading
  if ((registry_fifo_fd = open(registry_fifo_path, O_RDONLY)) == -1) {
    fprintf(stderr, "Failed to open the named pipe (FIFO).\n");
    pthread_exit(NULL);
  }

  while (atomic_load(&connection_listener_alive)) {
    char buffer[MAX_STRING_SIZE];
    ssize_t bytes_read = read(registry_fifo_fd, buffer, MAX_STRING_SIZE);
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

        struct ClientFIFOs* client_data = malloc(sizeof(struct ClientFIFOs));
        client_data->req_pipe_path = req_pipe_path;
        client_data->resp_pipe_path = resp_pipe_path;
        client_data->notif_pipe_path = notif_pipe_path;

        // Wait on the semaphore if there is no space left
        sem_wait(&max_clients);

        // Create a thread to handle client requests
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_request_listener, client_data) != 0) {
          perror("pthread_create");
          free(client_data);
          sem_post(&max_clients);
        }
       add_client_thread(client_thread, client_data);
      }
    }
  }
  close(registry_fifo_fd);
  unlink(registry_fifo_path);
  clear_all_subscriptions();
  join_all_client_threads();
  pthread_exit(NULL);
}
