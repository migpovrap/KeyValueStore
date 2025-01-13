#ifndef MACROS_H
#define MACROS_H

#include <errno.h>

#define TYPECHECK_ARG(arg) ({ \
  char *endptr; \
  strtol((arg), &endptr, 10); \
  *endptr != '\0'; \
})

#define CHECK_NULL(value, message) \
  do { \
    if ((value) == NULL) { \
      fprintf(stderr, "%s - %s\n", message, strerror(errno)); \
      exit(1); \
    } \
  } while (0)

#define CHECK_NOT_NULL(value, message) \
  do { \
    if ((value) != NULL) { \
      fprintf(stderr, "%s - %s\n", message, strerror(errno)); \
      exit(1); \
    } \
  } while (0)

#define CHECK_RETURN_ONE(value, message) \
  do { \
    if ((value) == 1) { \
      fprintf(stderr, "%s - %s\n", message, strerror(errno)); \
      exit(1); \
    } \
  } while (0)

#define CHECK_RETURN_MINUS_ONE(value, message) \
  do { \
    if ((value) == -1) { \
      fprintf(stderr, "%s - %s\n", message, strerror(errno)); \
      exit(1); \
    } \
  } while (0)

#define CHECK_NUM_PAIRS(value, message) \
  do { \
    if ((value) == 0) { \
      fprintf(stderr, "%s - %s\n", message, strerror(errno)); \
      exit(1); \
    } \
  } while (0)

#endif
