#include "server/utils.h"

extern ServerData* server_data;

void initialize_server_data(char* job_path,
char* max_threads, char* max_backups) {
  char* endptr;
  server_data->max_backups = strtoul(max_backups, &endptr, 10);
  if (*endptr != '\0') {
    write_str(STDERR_FILENO, "Invalid max_backups value.\n");
    cleanup_and_exit(1);
  } else if (*max_backups <= 0) {
		write_str(STDERR_FILENO, "Invalid number of backups.\n");
		cleanup_and_exit(1);
	}
  server_data->max_threads = strtoul(max_threads, &endptr, 10);
  if (*endptr != '\0') {
		write_str(STDERR_FILENO, "Invalid max_threads value.\n");
    cleanup_and_exit(1);
  } else if (*max_threads <= 0) {
		write_str(STDERR_FILENO, "Invalid number of threads.\n");
		cleanup_and_exit(1);
	}

  pthread_mutex_init(&server_data->all_subscriptions.mutex, NULL);
  sem_init(&server_data->backup_semaphore, 0, (unsigned int)server_data->max_backups); // 0 means semaphore is shared between threads of the same process
  server_data->all_subscriptions.subscription_data = NULL;
  server_data->jobs_directory = job_path;
  server_data->sigusr1_received = 0;
  server_data->child_terminated_flag = 0;
  server_data->terminate = 0;
}

void handle_sigusr1() {
  server_data->sigusr1_received = 1;
}

void handle_sigint_sigterm() {
  server_data->terminate = 1;
}

void setup_signal_handling() {
  // SIGUSR1
  struct sigaction sa_usr1;
  sa_usr1.sa_handler = handle_sigusr1;
  sigemptyset(&sa_usr1.sa_mask);
  sa_usr1.sa_flags = 0;
  sigaction(SIGUSR1, &sa_usr1, NULL);

  // SIGINT & SIGTERM
  struct sigaction sa;
  sa.sa_handler = handle_sigint_sigterm;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // Sigaction struct to handle SIGCHLD signals
  struct sigaction sa_child;
  sa_child.sa_handler = signal_child_terminated;
  sigemptyset(&sa_child.sa_mask); // Initialize the signal set to empty
  sa_child.sa_flags = SA_RESTART; // Restart interrupted system calls
  CHECK_RETURN_ONE(sigaction(SIGCHLD, &sa_child, NULL), "sigaction");
}

int setup_registration_fifo(char* registration_fifo_path) {
  if (pthread_create(&server_data->connection_manager, NULL,
  connection_manager, registration_fifo_path) != 0) {
    write_str(STDERR_FILENO, "Failed to create connection manager thread.\n");
    return 1;
  }
  return 0;
}

int setup_client_workers() {
  // Allocate memory for the worker threads array.
  server_data->worker_threads = malloc(MAX_SESSION_COUNT * sizeof(pthread_t));
  if (server_data->worker_threads == NULL) {
    write_str(STDERR_FILENO, "Failed to allocate memory for worker threads.\n");
    cleanup_and_exit(1);
  }
  // Create a pool of worker threads.
  for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
    if (pthread_create(&server_data->worker_threads[i], NULL,
    client_request_handler, NULL) != 0) {
      write_str(STDERR_FILENO, "Failed to create worker thread.\n");
      cleanup_and_exit(1);
    }
  }
  return 0;
}

void cleanup_and_exit(int exit_code) {
  if (server_data) {
    // Signal the connection manager thread to exit if it was created.
    if (atomic_load(&server_data->terminate)) {
      atomic_store(&server_data->terminate, 1);
      pthread_join(server_data->connection_manager, NULL);
    }

    // Join worker threads if they were created.
    if (server_data->worker_threads != NULL) {
      for (int i = 0; i < MAX_SESSION_COUNT; ++i) {
        if (server_data->worker_threads[i] != 0) {
          pthread_cancel(server_data->worker_threads[i]);
          pthread_join(server_data->worker_threads[i], NULL);
        }
      }
      // Free the worker threads array.
      free(server_data->worker_threads);
      cleanup_session_buffer();
    }

    // Destroy mutexes
    pthread_mutex_destroy(&server_data->all_subscriptions.mutex);
  }
  kvs_terminate();
  free(server_data);
  _exit(exit_code);
}

void run_jobs() {
  JobQueue *queue = create_job_queue(server_data->jobs_directory);
  CHECK_NULL(queue, "Failed to create job queue.");
  int max_threads = (int) server_data->max_threads;
  int max_files = queue->num_files;
  pthread_t threads[max_threads];

  pthread_t semaphore_thread;
  pthread_create(&semaphore_thread, NULL, checks_for_terminated_children, NULL);

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_create(&threads[i], NULL, process_file, (void *)queue);

  for (int i = 0; i < max_threads && i < max_files; ++i)
    pthread_join(threads[i], NULL);

  while (sem_trywait(&server_data->backup_semaphore) != 0) {
    struct timespec delay = delay_to_timespec(1);
    nanosleep(&delay, NULL);
  }
  // Very small wait to avoid missing signals or exit before child cleanup.
  struct timespec delay = delay_to_timespec(1);
  nanosleep(&delay, NULL);

  pthread_cancel(semaphore_thread);
  pthread_join(semaphore_thread, NULL);

  sem_destroy(&server_data->backup_semaphore);
  destroy_jobs_queue(queue);
  queue = NULL;
  return;
}