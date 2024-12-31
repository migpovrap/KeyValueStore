#include "client/utils.h"

extern ClientData* client_data;

void initialize_client_data(char* client_id) {
  snprintf(client_data->req_pipe_path, sizeof(client_data->req_pipe_path),
  "/tmp/req%s", client_id);
  snprintf(client_data->resp_pipe_path, sizeof(client_data->resp_pipe_path),
  "/tmp/resp%s", client_id);
  snprintf(client_data->notif_pipe_path, sizeof(client_data->notif_pipe_path),
  "/tmp/notif%s", client_id);
  client_data->req_fifo_fd = -1;
  client_data->resp_fifo_fd = -1;
  client_data->notif_fifo_fd = -1;
}

/* Cancel notification thread, unlink FIFOs,
 close file descriptors and free struct. */
void cleanup() {
  if (client_data) {
    if (client_data->notif_thread != 0) {
      pthread_cancel(client_data->notif_thread);
      pthread_join(client_data->notif_thread, NULL);
    }

    if (client_data->req_fifo_fd != -1) close(client_data->req_fifo_fd);
    if (client_data->resp_fifo_fd != -1) close(client_data->resp_fifo_fd);
    if (client_data->notif_fifo_fd != -1) close(client_data->notif_fifo_fd);

    // Unlink FIFOs only if they were created.
    if (access(client_data->req_pipe_path, F_OK) == 0)
      unlink(client_data->req_pipe_path);
    if (access(client_data->resp_pipe_path, F_OK) == 0)
      unlink(client_data->resp_pipe_path);
    if (access(client_data->notif_pipe_path, F_OK) == 0)
      unlink(client_data->notif_pipe_path);

    free(client_data);
    client_data = NULL;
  }
}

// SIGINT and SIGTERM
void signal_handler() {
  if (client_data)
    client_data->terminate = 1;
}

void setup_signal_handling() {
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL); 
}

void check_terminate_signal() {
  if (client_data && atomic_load(&client_data->terminate))
    exit(0);
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

// Thread function.
void* notification_listener() {
  char buffer[MAX_STRING_SIZE];
  while (!atomic_load(&client_data->terminate)) {
    int notif_fifo_fd = client_data->notif_fifo_fd;

    if (notif_fifo_fd == -1) break;

    int bytes_read = read_string(notif_fifo_fd, buffer);
    
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      printf("Notification: %s\n", buffer);
    } else if (bytes_read == 0) {
      fprintf(stderr, "Notification pipe closed by server.\n");
      kill(getpid(), SIGINT);
      pthread_exit(NULL);
    } else if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        sleep(1);
        continue;
      } else {
        fprintf(stderr, "Error reading notification pipe.\n");
        break;
      }
    }
  }
  return NULL;
}
