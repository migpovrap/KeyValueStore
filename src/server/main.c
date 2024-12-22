#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

#include "constants.h"
#include "common/protocol.h"
#include "parser.h"
#include "operations.h"
#include "io.h"
#include "pthread.h"

struct SharedData {
  DIR* dir;
  char* dir_name;
  pthread_mutex_t directory_mutex;
};

struct ClientFIFOs {
  char* req_pipe_path; 
  char* resp_pipe_path; 
  char* notif_pipe_path; 
};

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t n_current_backups_lock = PTHREAD_MUTEX_INITIALIZER;

size_t active_backups = 0;     // Number of active backups
size_t max_backups;            // Maximum allowed simultaneous backups
size_t max_threads;            // Maximum allowed simultaneous threads
char* jobs_directory = NULL;   // Directory containing the jobs files
char* registry_fifo_path = NULL;    // Registry fifo name

int filter_job_files(const struct dirent* entry) {
    const char* dot = strrchr(entry->d_name, '.');
    if (dot != NULL && strcmp(dot, ".job") == 0) {
        return 1;  // Keep this file (it has the .job extension)
    }
    return 0;
}

static int entry_files(const char* dir, struct dirent* entry, char* in_path, char* out_path) {
  const char* dot = strrchr(entry->d_name, '.');
  if (dot == NULL || dot == entry->d_name || strlen(dot) != 4 || strcmp(dot, ".job")) {
    return 1;
  }

  if (strlen(entry->d_name) + strlen(dir) + 2 > MAX_JOB_FILE_NAME_SIZE) {
    fprintf(stderr, "%s/%s\n", dir, entry->d_name);
    return 1;
  }

  strcpy(in_path, dir);
  strcat(in_path, "/");
  strcat(in_path, entry->d_name);

  strcpy(out_path, in_path);
  strcpy(strrchr(out_path, '.'), ".out");

  return 0;
}

static int run_job(int in_fd, int out_fd, char* filename) {
  size_t file_backups = 0;
  while (1) {
    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    unsigned int delay;
    size_t num_pairs;

    switch (get_next(in_fd)) {
      case CMD_WRITE:
        num_pairs = parse_write(in_fd, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_write(num_pairs, keys, values)) {
          write_str(STDERR_FILENO, "Failed to write pair\n");
        }
        break;

      case CMD_READ:
        num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_read(num_pairs, keys, out_fd)) {
          write_str(STDERR_FILENO, "Failed to read pair\n");
        }
        break;

      case CMD_DELETE:
        num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_delete(num_pairs, keys, out_fd)) {
          write_str(STDERR_FILENO, "Failed to delete pair\n");
        }
        break;

      case CMD_SHOW:
        kvs_show(out_fd);
        break;

      case CMD_WAIT:
        if (parse_wait(in_fd, &delay, NULL) == -1) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay > 0) {
          printf("Waiting %d seconds\n", delay / 1000);
          kvs_wait(delay);
        }
        break;

      case CMD_BACKUP:
        pthread_mutex_lock(&n_current_backups_lock);
        if (active_backups >= max_backups) {
          wait(NULL);
        } else {
          active_backups++;
        }
        pthread_mutex_unlock(&n_current_backups_lock);
        int aux = kvs_backup(++file_backups, filename, jobs_directory);

        if (aux < 0) {
            write_str(STDERR_FILENO, "Failed to do backup\n");
        } else if (aux == 1) {
          return 1;
        }
        break;

      case CMD_INVALID:
        write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        write_str(STDOUT_FILENO,
            "Available commands:\n"
            "  WRITE [(key,value)(key2,value2),...]\n"
            "  READ [key,key2,...]\n"
            "  DELETE [key,key2,...]\n"
            "  SHOW\n"
            "  WAIT <delay_ms>\n"
            "  BACKUP\n" // Not implemented
            "  HELP\n");

        break;

      case CMD_EMPTY:
        break;

      case EOC:
        printf("EOF\n");
        return 0;
    }
  }
}

//frees arguments
static void* get_file(void* arguments) {
  struct SharedData* thread_data = (struct SharedData*) arguments;
  DIR* dir = thread_data->dir;
  char* dir_name = thread_data->dir_name;

  if (pthread_mutex_lock(&thread_data->directory_mutex) != 0) {
    fprintf(stderr, "Thread failed to lock directory_mutex\n");
    return NULL;
  }

  struct dirent* entry;
  char in_path[MAX_JOB_FILE_NAME_SIZE], out_path[MAX_JOB_FILE_NAME_SIZE];
  while ((entry = readdir(dir)) != NULL) {
    if (entry_files(dir_name, entry, in_path, out_path)) {
      continue;
    }

    if (pthread_mutex_unlock(&thread_data->directory_mutex) != 0) {
      fprintf(stderr, "Thread failed to unlock directory_mutex\n");
      return NULL;
    }

    int in_fd = open(in_path, O_RDONLY);
    if (in_fd == -1) {
      write_str(STDERR_FILENO, "Failed to open input file: ");
      write_str(STDERR_FILENO, in_path);
      write_str(STDERR_FILENO, "\n");
      pthread_exit(NULL);
    }

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd == -1) {
      write_str(STDERR_FILENO, "Failed to open output file: ");
      write_str(STDERR_FILENO, out_path);
      write_str(STDERR_FILENO, "\n");
      pthread_exit(NULL);
    }

    int out = run_job(in_fd, out_fd, entry->d_name);

    close(in_fd);
    close(out_fd);

    if (out) {
      if (closedir(dir) == -1) {
        fprintf(stderr, "Failed to close directory\n");
        return 0;
      }

      exit(0);
    }

    if (pthread_mutex_lock(&thread_data->directory_mutex) != 0) {
      fprintf(stderr, "Thread failed to lock directory_mutex\n");
      return NULL;
    }
  }

  if (pthread_mutex_unlock(&thread_data->directory_mutex) != 0) {
    fprintf(stderr, "Thread failed to unlock directory_mutex\n");
    return NULL;
  }

  pthread_exit(NULL);
}

void* client_request_listener(void* args) {
  struct ClientFIFOs* client_data = (struct ClientFIFOs*)args;
  int request_fifo_fd = open(client_data->req_pipe_path, O_RDONLY);
  int response_fifo_fd = open(client_data->resp_pipe_path, O_WRONLY);
  int notification_fifo_fd;

  if (request_fifo_fd == -1 || response_fifo_fd == -1) {
    fprintf(stderr, "Failed to open the client fifos.\n");
    pthread_exit(NULL);
  } 

  // Send result op code to to client
  char response[2] = {OP_CODE_CONNECT, 0}; // 0 indicates success
  if (write(response_fifo_fd, response, 2) == -1) {
    fprintf(stderr, "Failed to write to the response fifo of the client.\n");
  }

  char buffer[MAX_STRING_SIZE];

  notification_fifo_fd = open(client_data->notif_pipe_path, O_WRONLY); // DEBUG LINE REMOVE

  while (1) {
    // DEBUG CODE REMOVE
    char noti[10] = {"TESTE"};
    write(notification_fifo_fd, noti, 10);
    printf("Sending notif.\n");
    // END REMOVE
    ssize_t bytes_read = read(request_fifo_fd, buffer, MAX_STRING_SIZE);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';

      // Parse the client operation code request 
      char* token = strtok(buffer, "|");
      int op_code_int = atoi(token);
      enum OperationCode op_code = (enum OperationCode)op_code_int;

      switch (op_code) {
        case OP_CODE_SUBSCRIBE:
          // Handle the subscribe request
          break;
        case OP_CODE_UNSUBSCRIBE:
          // Handle the unsubscribe request
          break;
        case OP_CODE_DISCONNECT:
          // Remove all subscriptions on the hashtable
          // Send result op code to to client
          char response[2] = {OP_CODE_DISCONNECT, 0}; // 0 indicates success
          if (write(response_fifo_fd, response, 2) == -1) {
            fprintf(stderr, "Failed to write to the response fifo of the client.\n");
          }
          // Closes the client fifos (open on the server)
          close(request_fifo_fd);
          close(response_fifo_fd);
          close(notification_fifo_fd);
          pthread_exit(NULL);
          break;
        default:
          fprintf(stderr, "Unknown operation code: %d\n", op_code);
          break;
      }
    }
  }

  close(request_fifo_fd);
  pthread_exit(NULL);
}

void* connection_listener(void* args) {
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
  pthread_t client_thread;
  //TODO Needs a loop to wait for a slot to accepts connections and control max number of connections
  char buffer[MAX_STRING_SIZE];
  ssize_t bytes_read = read(registry_fifo_fd, buffer, MAX_STRING_SIZE);
  if (bytes_read > 0) {
    buffer[bytes_read] = '\0';

    // Parse the client operation code request 
    char* token = strtok(buffer, "|");
    int op_code_int = atoi(token);
    enum OperationCode op_code = (enum OperationCode)op_code_int;

    switch (op_code) {
      case OP_CODE_CONNECT: {
        char* req_pipe_path = strtok(NULL, "|");
        char* resp_pipe_path = strtok(NULL, "|");
        char* notif_pipe_path = strtok(NULL, "|");

        struct ClientFIFOs client_data = {req_pipe_path, resp_pipe_path, notif_pipe_path};

        // Create a thread to handle client requests
        if (pthread_create(&client_thread, NULL, client_request_listener, &client_data) != 0) {
          perror("pthread_create");
        }
        // For now the it exist in the disconnect command.
        // TODO: Make a list of clients and the thread that is handeling their request
        // TODO: Create a new thread that runs a fucntion to handle all client request. (This way it implements the two parts in one go)
      break;
      }

      default:
        fprintf(stderr, "Unknown operation code: %s\n", token);
      break;
    }
  }
  pthread_join(client_thread, NULL);
  close(registry_fifo_fd);
  unlink(registry_fifo_path);
  pthread_exit(NULL);
}

static void dispatch_threads(DIR* dir) {
  pthread_t* threads = malloc(max_threads * sizeof(pthread_t));

  if (threads == NULL) {
    fprintf(stderr, "Failed to allocate memory for threads\n");
    return;
  }

  struct SharedData thread_data = {dir, jobs_directory, PTHREAD_MUTEX_INITIALIZER};


  for (size_t i = 0; i < max_threads; i++) {
    if (pthread_create(&threads[i], NULL, get_file, (void*)&thread_data) != 0) {
      fprintf(stderr, "Failed to create thread %zu\n", i);
      pthread_mutex_destroy(&thread_data.directory_mutex);
      free(threads);
      return;
    }
  }

  // ler do FIFO de registo

  for (unsigned int i = 0; i < max_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Failed to join thread %u\n", i);
      pthread_mutex_destroy(&thread_data.directory_mutex);
      free(threads);
      return;
    }
  }

  if (pthread_mutex_destroy(&thread_data.directory_mutex) != 0) {
    fprintf(stderr, "Failed to destroy directory_mutex\n");
  }

  free(threads);
}


int main(int argc, char** argv) {
  if (argc < 4) {
    write_str(STDERR_FILENO, "Usage: ");
    write_str(STDERR_FILENO, argv[0]);
    write_str(STDERR_FILENO, " <jobs_dir>");
		write_str(STDERR_FILENO, " <max_threads>");
		write_str(STDERR_FILENO, " <max_backups>");
    write_str(STDERR_FILENO, " <registry_fifo_path> \n");
    return 1;
  }

  jobs_directory = argv[1];
  registry_fifo_path = argv[4];

  char* endptr;
  max_backups = strtoul(argv[3], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_proc value\n");
    return 1;
  }

  max_threads = strtoul(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_threads value\n");
    return 1;
  }

	if (max_backups <= 0) {
		write_str(STDERR_FILENO, "Invalid number of backups\n");
		return 0;
	}

	if (max_threads <= 0) {
		write_str(STDERR_FILENO, "Invalid number of threads\n");
		return 0;
	}

  if (kvs_init()) {
    write_str(STDERR_FILENO, "Failed to initialize KVS\n");
    return 1;
  }

  DIR* dir = opendir(argv[1]);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory: %s\n", argv[1]);
    return 0;
  }
  
  // Create a thread to listen on the registry fifo non blocking.
  pthread_t registry_listener;
  if (pthread_create(&registry_listener, NULL, connection_listener, registry_fifo_path) != 0) {
    fprintf(stderr, "Failed to create connection manager thread\n");
    return 1;
  }

  dispatch_threads(dir);

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory\n");
    return 0;
  }

  while (active_backups > 0) {
    wait(NULL);
    active_backups--;
  }

  pthread_join(registry_listener, NULL);

  kvs_terminate();

  return 0;
}
