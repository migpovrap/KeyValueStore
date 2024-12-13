#include "parser.h"

static int read_string(int fd, char *buffer, size_t max) {
  ssize_t bytes_read;
  char ch;
  size_t i = 0;
  int value = -1;

  while (i < max) {
    bytes_read = read(fd, &ch, 1);

    if (bytes_read <= 0) 
      return -1;

    if (ch == ' ')
      return -1;
  
    if (ch == ',') {
      value = 0;
      break;
    } else if (ch == ')') {
      value = 1;
      break;
    } else if (ch == ']') {
      value = 2;
      break;
    }

    buffer[i++] = ch;
  }

  buffer[i] = '\0';

  return value;
}

static int read_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    if (read(fd, buf + i, 1) == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

static void cleanup(int fd) {
  char ch;
  while (read(fd, &ch, 1) == 1 && ch != '\n');
}

enum Command parse_w(int fd, char buff[16]) {
  if (read(fd, buff + 1, 4) != 4 || strncmp(buff, "WAIT ", 5) != 0) {
    if (read(fd, buff + 5, 1) != 1 || strncmp(buff, "WRITE ", 6) != 0) {
      cleanup(fd);
      return CMD_INVALID;
    }
    return CMD_WRITE;
  }
  return CMD_WAIT;
}

enum Command parse_r(int fd, char buff[16]) {
  if (read(fd, buff + 1, 4) != 4 || strncmp(buff, "READ ", 5) != 0) {
    cleanup(fd);
    return CMD_INVALID;
  }
  return CMD_READ;
}

enum Command parse_d(int fd, char buff[16]) {
  if (read(fd, buff + 1, 6) != 6 || strncmp(buff, "DELETE ", 7) != 0) {
    cleanup(fd);
    return CMD_INVALID;
  }
  return CMD_DELETE;
}

enum Command parse_s(int fd, char buff[16]) {
  if (read(fd, buff + 1, 3) != 3 || strncmp(buff, "SHOW", 4) != 0) {
    cleanup(fd);
    return CMD_INVALID;
  }
  if (read(fd, buff + 4, 1) != 0 && buff[4] != '\n') {
    cleanup(fd);
    return CMD_INVALID;
  }
  return CMD_SHOW;
}

enum Command parse_b(int fd, char buff[16]) {
  if (read(fd, buff + 1, 5) != 5 || strncmp(buff, "BACKUP", 6) != 0) {
    cleanup(fd);
    return CMD_INVALID;
  }
  if (read(fd, buff + 6, 1) != 0 && buff[6] != '\n') {
    cleanup(fd);
    return CMD_INVALID;
  }
  return CMD_BACKUP;
}

enum Command parse_h(int fd, char buff[16]) {
  if (read(fd, buff + 1, 3) != 3 || strncmp(buff, "HELP", 4) != 0) {
    cleanup(fd);
    return CMD_INVALID;
  }
  if (read(fd, buff + 4, 1) != 0 && buff[4] != '\n') {
    cleanup(fd);
    return CMD_INVALID;
  }
  return CMD_HELP;
}

enum Command get_next(int fd) {
  char buff[16];
  if (read(fd, buff, 1) != 1)
    return EOC;
  switch (buff[0]) {
    case 'W':
      return parse_w(fd, buff);
    case 'R':
      return parse_r(fd, buff);
    case 'D':
      return parse_d(fd, buff);
    case 'S':
      return parse_s(fd, buff);
    case 'B':
      return parse_b(fd, buff);
    case 'H':
      return parse_h(fd, buff);
    case '#':
      cleanup(fd);
      return CMD_EMPTY;
    case '\n':
      return CMD_EMPTY;
    default:
      cleanup(fd);
      return CMD_INVALID;
  }
}

int parse_pair(int fd, char *key, char *value) {
  if (read_string(fd, key, MAX_STRING_SIZE) != 0) {
    cleanup(fd);
    return 0;
  }

  if (read_string(fd, value, MAX_STRING_SIZE) != 1) {
    cleanup(fd);
    return 0;
  }
  return 1;
}

size_t parse_write(int fd, char keys[][MAX_STRING_SIZE],
  char values[][MAX_STRING_SIZE], size_t max_pairs, size_t max_string_size) {
  char ch;

  if (read(fd, &ch, 1) != 1 || ch != '[') {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || ch != '(') {
    cleanup(fd);
    return 0;
  }

  size_t num_pairs = 0;
  char key[max_string_size];
  char value[max_string_size];
  while (num_pairs < max_pairs) {
    if(parse_pair(fd, key, value) == 0) {
      cleanup(fd);
      return 0;
    }

    strcpy(keys[num_pairs], key);
    strcpy(values[num_pairs++], value);

    if (read(fd, &ch, 1) != 1 || (ch != '(' && ch != ']')) {
      cleanup(fd);
      return 0;
    }

    if (ch == ']')
      break;
  }

  if (num_pairs == max_pairs) {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 0;
  }

  return num_pairs;
}

size_t parse_read_delete(int fd, char keys[][MAX_STRING_SIZE],
  size_t max_keys, size_t max_string_size) {
  char ch;

  if (read(fd, &ch, 1) != 1 || ch != '[') {
    cleanup(fd);
    return 0;
  }

  size_t num_keys = 0;
  char key[max_string_size];
  while (num_keys < max_keys) {
    int output = read_string(fd, key, max_string_size);
    if(output < 0 || output == 1) {
      cleanup(fd);
      return 0;
    }

    strcpy(keys[num_keys++], key);

    if (output == 2)
      break;
  }

  if (num_keys == max_keys) {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 0;
  }

  return num_keys;
}

int parse_wait(int fd, unsigned int *delay, unsigned int *thread_id) {
  char ch;

  if (read_uint(fd, delay, &ch) != 0) {
    cleanup(fd);
    return -1;
  }

  if (ch == ' ') {
    if (thread_id == NULL) {
      cleanup(fd);
      return 0;
    }

    if (read_uint(fd, thread_id, &ch) != 0 || (ch != '\n' && ch != '\0')) {
      cleanup(fd);
      return -1;
    }

    return 1;
  } else if (ch == '\n' || ch == '\0') {
    return 0;
  } else {
    cleanup(fd);
    return -1;
  }
}
