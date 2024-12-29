#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "client/api.h"
#include "common/constants.h"
#include "common/io.h"
#include "parser.h"
#include "utils.c"

ClientData* client_data;
pthread_mutex_t client_mutex;

int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }
  
  client_data = malloc(sizeof(ClientData));
  if (!client_data) {
    fprintf(stderr, "Failed to allocate memory for client data.\n");
    return 1;
  }

  initialize_client_data(argv[1]);
  pthread_mutex_init(&client_mutex, NULL);

  setup_signal_handling();

  if (create_fifos() != 0) {
    cleanup_and_exit(1);
  }

  // Call FIFO cleanup at normal program exit
  atexit(cleanup_fifos);

  // Connect to the server
  if (kvs_connect(client_data, argv[2]) != 0) {
    fprintf(stderr, "Failed to connect to the KVS server.\n");
    cleanup_and_exit(1);
  }

  // Create a thread for notifications
  if (pthread_create(&client_data->notif_thread, NULL, notification_listener, NULL) != 0) {
    perror("pthread_create");
    cleanup_and_exit(1);
  }

  // Set terminate when server closes fifos, notification_listener thread. Also "atomic_load"?
  while (!client_data->terminate) {
    char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
    unsigned int delay_ms;
    size_t num;

    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect(client_data) != 0) {
          fprintf(stderr, "Failed to disconnect from the server.\n");
          cleanup_and_exit(1);
        }
        pthread_cancel(client_data->notif_thread);
        pthread_join(client_data->notif_thread, NULL);
        printf("Disconnected from server.\n");
        cleanup_and_exit(0);
        break;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          continue;
        }
        if (kvs_subscribe(client_data, keys[0])) {
          fprintf(stderr, "Command subscribe failed.\n");
        }
        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          continue;
        }
        if (kvs_unsubscribe(client_data, keys[0])) {
          fprintf(stderr, "Command unsubscribe failed.\n");
        }
        break;

      case CMD_DELAY:
        if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage.\n");
          continue;
        }
        if (delay_ms > 0) {
          printf("Waiting...\n");
          delay(delay_ms);
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage.\n");
        break;

      case CMD_EMPTY:
        break;

      case EOC:
        // Input should end in a disconnect, or it will loop here forever.
        break;
    }
  }
}
