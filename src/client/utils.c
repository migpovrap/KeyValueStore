#include <errno.h>
#include <stdatomic.h>

#include "api.h"
#include "utils.h"

extern ClientData* client_data;
volatile sig_atomic_t terminate_flag = 0;


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
}

// Signal handler for SIGINT and SIGTERM
void signal_handler() {
  terminate_flag = 1;
}

void check_terminate_signal() {
  if (terminate_flag) {
    if (client_data) {
      client_data->terminate = 1;
      pthread_cancel(client_data->notif_thread);
      pthread_join(client_data->notif_thread, NULL);
    }
    cleanup_fifos();
    _exit(0);
  }
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

// Notification listener thread function
void* notification_listener() {
  char buffer[MAX_STRING_SIZE];
  while (1) {
    int bytes_read = read_string(client_data->notif_fifo_fd, buffer);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        sleep(1);
        continue;
      } else {
        fprintf(stderr, "Error reading notification pipe.\n");
        break;
      }
    }
    if (bytes_read == 0) {
      fprintf(stderr, "Notification pipe closed by server.\n");
      kill(getpid(), SIGINT);
      pthread_exit(NULL);
    }
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      printf("Notification: %s\n", buffer);
    }
  }
  close(client_data->notif_fifo_fd);
  return NULL;
}

void cleanup_and_exit(int exit_code) {
  cleanup_fifos();
  exit(exit_code);
}
