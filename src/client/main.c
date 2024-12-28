#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>

#include "parser.h"
#include "client/api.h"
#include "common/constants.h"
#include "common/io.h"

// Global variables for FIFO paths and thread management
char req_pipe_path[MAX_PIPE_PATH_LENGTH];
char resp_pipe_path[MAX_PIPE_PATH_LENGTH];
char notif_pipe_path[MAX_PIPE_PATH_LENGTH];
pthread_t notification_thread;
int request_fifo_fd = -1, response_fifo_fd = -1, notification_fifo_fd = -1;

// Function to clean up FIFOs and close file descriptors
void cleanup_fifos() {
  if (request_fifo_fd != -1) close(request_fifo_fd);
  if (response_fifo_fd != -1) close(response_fifo_fd);
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);
  printf("Cleaned up FIFOs.\n");
}

// Signal handler for SIGINT and SIGTERM
void signal_handler(int signo) {
  fprintf(stderr, "Received signal %d. Exiting...\n", signo);
  cleanup_fifos();
  pthread_cancel(notification_thread);
  pthread_join(notification_thread, NULL);
  exit(0);
}

// Notification listener thread function
void* notification_listener(void* arg) {
  int notification_fifo_fd = *(int*)arg;
  char buffer[MAX_STRING_SIZE];
  while (1) {
    ssize_t bytes_read = read(notification_fifo_fd, buffer, MAX_STRING_SIZE);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        sleep(1);
        continue;
      } else {
        perror("read");
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

  close(notification_fifo_fd);
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }

  snprintf(req_pipe_path, sizeof(req_pipe_path), "/tmp/req%s", argv[1]);
  snprintf(resp_pipe_path, sizeof(resp_pipe_path), "/tmp/resp%s", argv[1]);
  snprintf(notif_pipe_path, sizeof(notif_pipe_path), "/tmp/notif%s", argv[1]);

  // Register FIFO cleanup for normal program exit
  atexit(cleanup_fifos);

  // Set up signal handlers for SIGINT and SIGTERM
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // Create FIFOs
  if (mkfifo(req_pipe_path, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo req_pipe");
    return 1;
  }
  if (mkfifo(resp_pipe_path, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo resp_pipe");
    return 1;
  }
  if (mkfifo(notif_pipe_path, 0666) == -1 && errno != EEXIST) {
    perror("mkfifo notif_pipe");
    return 1;
  }

  // Connect to the server
  if (kvs_connect(req_pipe_path, resp_pipe_path, notif_pipe_path, argv[2], &request_fifo_fd, &response_fifo_fd, &notification_fifo_fd)) {
    fprintf(stderr, "Failed to connect to the KVS server.\n");
    return 1;
  }

  // Create a thread for notifications
  if (pthread_create(&notification_thread, NULL, notification_listener, &notification_fifo_fd) != 0) {
    perror("pthread_create");
    return 1;
  }

  while (1) {
    char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
    unsigned int delay_ms;
    size_t num;

    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect(&request_fifo_fd, &response_fifo_fd) != 0) {
          fprintf(stderr, "Failed to disconnect from the server\n");
          return 1;
        }
        pthread_cancel(notification_thread);
        pthread_join(notification_thread, NULL);
        printf("Disconnected from server\n");
        return 0;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (kvs_subscribe(&request_fifo_fd, &response_fifo_fd, keys[0])) {
          fprintf(stderr, "Command subscribe failed\n");
        }
        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (kvs_unsubscribe(&request_fifo_fd, &response_fifo_fd, keys[0])) {
          fprintf(stderr, "Command unsubscribe failed\n");
        }
        break;

      case CMD_DELAY:
        if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (delay_ms > 0) {
          printf("Waiting...\n");
          delay(delay_ms);
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_EMPTY:
        break;

      case EOC:
        // input should end in a disconnect, or it will loop here forever
        break;
    }
  }
}
