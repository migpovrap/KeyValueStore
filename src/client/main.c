#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "parser.h"
#include "client/api.h"
#include "common/constants.h"
#include "common/io.h"

void* notification_listener(void* arg) {
  char* notif_pipe_path = (char*)arg;
  int notification_fifo_fd = open(notif_pipe_path, O_RDONLY);
  if (notification_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the notification named pipe (FIFO).\n");
    pthread_exit(NULL);
  }
  char buffer[MAX_STRING_SIZE];

  while (1) {
    ssize_t bytes_read = read(notification_fifo_fd, buffer, MAX_STRING_SIZE);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      printf("Notification: %s\n", buffer);
    }
  }

  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }

  char req_pipe_path[256] = "/tmp/req";
  char resp_pipe_path[256] = "/tmp/resp";
  char notif_pipe_path[256] = "/tmp/notif";

  char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
  unsigned int delay_ms;
  size_t num;

  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  // Create the named pipes (FIFOS) of the client for comunication
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

  int request_fifo_fd, response_fifo_fd;

  // Open the connection to the kvs server
  if (kvs_connect(req_pipe_path, resp_pipe_path, notif_pipe_path, argv[2], &request_fifo_fd, &response_fifo_fd) != 0) {
    fprintf(stderr, "Failed to connect to the KVS server.\n");
    return 1;
  }

  // Create a thread to handle notifications
  pthread_t notification_thread;
  if (pthread_create(&notification_thread, NULL, notification_listener, &notif_pipe_path) != 0) {
    perror("pthread_create");
    return 1;
  }

  while (1) {
    switch (get_next(STDIN_FILENO)) {
      // Each command needs to print to stdout the response that it got from the server
      case CMD_DISCONNECT:
        if (kvs_disconnect(&request_fifo_fd, &response_fifo_fd) != 0) {
          fprintf(stderr, "Failed to disconnect to the server\n");
          return 1;
        }
        // Clean the notification thread
        pthread_cancel(notification_thread);
        pthread_join(notification_thread, NULL);
        unlink(req_pipe_path);
        unlink(resp_pipe_path);
        unlink(notif_pipe_path);
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
            fprintf(stderr, "Command subscribe failed\n");
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
