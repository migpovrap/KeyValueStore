#include "utils.h"
#include <errno.h>
#include "api.h"

extern ClientData* client_data;
extern pthread_mutex_t client_mutex;

void initialize_client_data(char* client_id) {
  client_data->req_fifo_fd = -1;
  client_data->resp_fifo_fd = -1;
  client_data->notif_fifo_fd = -1;
  client_data->client_subs = 0;
  client_data->terminate = 0;

  snprintf(client_data->req_pipe_path, sizeof(client_data->req_pipe_path), "/tmp/req%s", client_id);
  snprintf(client_data->resp_pipe_path, sizeof(client_data->resp_pipe_path), "/tmp/resp%s", client_id);
  snprintf(client_data->notif_pipe_path, sizeof(client_data->notif_pipe_path), "/tmp/notif%s", client_id);
}

// Function to unlink FIFOs and close file descriptors
void cleanup_fifos() {
  pthread_mutex_lock(&client_mutex);
  if (client_data) {
    if (client_data->req_fifo_fd != -1) close(client_data->req_fifo_fd);
    if (client_data->resp_fifo_fd != -1) close(client_data->resp_fifo_fd);
    if (client_data->notif_fifo_fd != -1) close(client_data->notif_fifo_fd);

    unlink(client_data->req_pipe_path);
    unlink(client_data->resp_pipe_path);
    unlink(client_data->notif_pipe_path);

    free(client_data);
    client_data = NULL;
  }
  pthread_mutex_unlock(&client_mutex);
  pthread_mutex_destroy(&client_mutex);
}

// Signal handler for SIGINT and SIGTERM
void signal_handler() {
  pthread_mutex_lock(&client_mutex);
  if (client_data) {
    client_data->terminate = 1;
    pthread_cancel(client_data->notif_thread);
    pthread_join(client_data->notif_thread, NULL);
  }
  pthread_mutex_unlock(&client_mutex);
  cleanup_fifos();
  _exit(0);
}

void setup_signal_handling() {
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

int create_fifos() {
  if (mkfifo(client_data->req_pipe_path, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo req_pipe");
    return 1;
  }
  if (mkfifo(client_data->resp_pipe_path, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo resp_pipe");
    return 1;
  }
  if (mkfifo(client_data->notif_pipe_path, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo notif_pipe");
    return 1;
  }
  return 0;
}

// Function assigned to the notification thread
void* notification_listener() {
  char buffer[MAX_STRING_SIZE];
  while (1) {
    pthread_mutex_lock(&client_mutex);
    if (!client_data || client_data->terminate) {
      pthread_mutex_unlock(&client_mutex);
      break;
    }
    ssize_t bytes_read = read(client_data->notif_fifo_fd, buffer, MAX_STRING_SIZE - 1);
    pthread_mutex_unlock(&client_mutex);

    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        sleep(1);
        continue;
      } else {
        perror("Error reading notification pipe.");
        break;
      }
    }
    if (bytes_read == 0) {
      fprintf(stderr, "Notification pipe closed by server.\n");
      break;
    }
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      printf("Notification: %s\n", buffer);
    }
  }
  return NULL;
}

void cleanup_and_exit(int exit_code) {
  cleanup_fifos();
  exit(exit_code);
}
