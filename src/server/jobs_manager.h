#ifndef JOBS_MANAGER_H
#define JOBS_MANAGER_H

#include <dirent.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef struct SharedData {
  DIR* dir;
  char* dir_name;
  pthread_mutex_t directory_mutex;
} SharedData;

/// Creates threads and releases them in the given directory to process jobs.
/// @param dir Directory where threads will look for jobs.
void dispatch_threads(DIR* dir);

#endif
