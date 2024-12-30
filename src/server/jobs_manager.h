#ifndef JOBS_MANAGER_H
#define JOBS_MANAGER_H

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

typedef struct SharedData {
  DIR* dir;
  char* dir_name;
  pthread_mutex_t directory_mutex;
} SharedData;

void dispatch_threads(DIR* dir);

#endif