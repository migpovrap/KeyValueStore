#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common/constants.h"

/// Reads a given number of bytes from a file descriptor. Will block until all
/// bytes are read, or fail if not all bytes could be read.
/// @param fd File descriptor to read from.
/// @param buffer Buffer to read into.
/// @param size Number of bytes to read.
/// @param intr Pointer to a variable that will be set to 1
/// if the read was interrupted.
/// @return On success, returns 1, on end of file, returns 0,
/// on error, returns -1
int read_all(int fd, void *buffer, size_t size, int *intr);

/// Reads a string from a file descriptor.
/// @param fd The file descriptor to read from.
/// @param str The buffer to store the read string.
/// @return The number of bytes read, or -1 on error.
int read_string(int fd, char *str);

/// Writes a given number of bytes to a file descriptor. Will block until all
/// bytes are written, or fail if not all bytes could be written.
/// @param fd File descriptor to write to.
/// @param buffer Buffer to write from.
/// @param size Number of bytes to write.
/// @return On success, returns 1, on error, returns -1
int write_all(int fd, const void *buffer, size_t size);

/// Delays the execution for a specified amount of time.
/// @param time_ms The amount of time to delay in milliseconds.
void delay(unsigned int time_ms);

#endif  // COMMON_IO_H
