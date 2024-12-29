#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "client/api.h"
#include "common/constants.h"
#include "common/io.h"
#include "parser.h"

// Global variables for FIFO paths file descriptors and notification thread
char req_pipe_path[MAX_PIPE_PATH_LENGTH];
char resp_pipe_path[MAX_PIPE_PATH_LENGTH];
char notif_pipe_path[MAX_PIPE_PATH_LENGTH];
int req_fifo_fd = -1, resp_fifo_fd = -1, notif_fifo_fd = -1;
pthread_t notif_thread;

// Function unlink FIFOs and close file descriptors
void cleanup_fifos() {
  if (req_fifo_fd != -1) close(req_fifo_fd);
  if (resp_fifo_fd != -1) close(resp_fifo_fd);
  if (notif_fifo_fd != -1) close (notif_fifo_fd);
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);
}

// Signal handler for SIGINT and SIGTERM
void signal_handler() {
  cleanup_fifos();
  pthread_cancel(notif_thread);
  pthread_join(notif_thread, NULL);
  exit(0);
}

// Notification listener thread function
void* notification_listener() {
  char buffer[MAX_STRING_SIZE];
  while (1) {
    int bytes_read =  read_string(notif_fifo_fd, buffer);
    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        sleep(1);
        continue;
      } else {
        perror("read_all");
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

  close(notif_fifo_fd);
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
  if (kvs_connect(req_pipe_path, resp_pipe_path, notif_pipe_path, argv[2], &req_fifo_fd, &resp_fifo_fd, &notif_fifo_fd)) {
    fprintf(stderr, "Failed to connect to the KVS server.\n");
    return 1;
  }

  // Create a thread for notifications
  if (pthread_create(&notif_thread, NULL, notification_listener, NULL) != 0) {
    perror("pthread_create");
    return 1;
  }

  while (1) {
    char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
    unsigned int delay_ms;
    size_t num;

    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect(&req_fifo_fd, &resp_fifo_fd) != 0) {
          fprintf(stderr, "Failed to disconnect from the server\n");
          return 1;
        }
        pthread_cancel(notif_thread);
        pthread_join(notif_thread, NULL);
        printf("Disconnected from server\n");
        return 0;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (kvs_subscribe(&req_fifo_fd, &resp_fifo_fd, keys[0])) {
          fprintf(stderr, "Command subscribe failed\n");
        }
        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        if (kvs_unsubscribe(&req_fifo_fd, &resp_fifo_fd, keys[0])) {
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
