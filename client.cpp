#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

const size_t k_max_msg = 4096;

static int32_t write_full(int fd, const char *buf, size_t count) {
  while (count > 0) {
    ssize_t n = write(fd, buf, count);
    if (n < 0) {
      return -1;
    }

    assert((ssize_t)n <= count);

    count -= (size_t)n;
    buf += n;
  }
  return 0;
}

static int32_t read_full(int fd, char *rbuf, size_t count) {
  while (count > 0) {
    ssize_t n = read(fd, rbuf, count);
    if (n <= 0) {
      return -1;
    }

    assert(ssize_t(n) <= count);

    count -= size_t(n);

    rbuf += n;
  }

  return 0;
}

static int32_t query(int fd, const char *text) {
  uint32_t len = (uint32_t)strlen(text);
  if (len > k_max_msg) {
    return -1;
  }

  char wbuf[4 + k_max_msg] = {};
  memcpy(wbuf, &len, sizeof(len));
  memcpy(wbuf + 4, text, len);

  int32_t err = write_full(fd, wbuf, 4 + len);

  if (err) {
    return err;
  }

  char rbuf[4 + k_max_msg] = {};
  int32_t err2 = read_full(fd, rbuf, 4);
  if (err2) {
    return err2;
  }

  mempcpy(&len, rbuf, sizeof(len));

  if (len > k_max_msg) {
    return -1;
  }

  int32_t err3 = read_full(fd, rbuf + 4, len);
  if (err3) {
    return err3;
  }

  printf("server says: %.*s\n", len, rbuf + 4);

  return 0;
}

int main() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    throw std::system_error(errno, std::system_category(), "socket() failed");
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1212);
  addr.sin_addr.s_addr = INADDR_ANY;

  int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));

  if (rv) {
    throw std::system_error(errno, std::system_category(), "connect() failed");
  }

  int32_t err = query(fd, "hello");
  if (err) {
    close(fd);
    return 0;
  }

  err = query(fd, "hello again");

  if (err) {
    close(fd);
    return 0;
  }

  close(fd);
}
