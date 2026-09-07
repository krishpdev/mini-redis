#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

const size_t k_max_msg = 4096;

static void throwsyserror(const char *msg) {
  throw std::system_error(errno, std::system_category(), msg);
}

static void msg(const char *msg) { fprintf(stderr, "%s\n", msg); }

static void do_something(int connfd) {
  char rbuf[64] = {};
  ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
  if (n < 0) {
    msg("read() error");
    return;
  }
  fprintf(stderr, "client says: %s\n", rbuf);

  char wbuf[] = "world";
  write(connfd, wbuf, strlen(wbuf));
}

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

static int32_t one_request(int connfd) {

  char rbuf[4 + k_max_msg] = {};
  errno = 0;
  int32_t err = read_full(connfd, rbuf, 4);
  if (err) {
    msg("read() error");
    return -1;
  }

  uint32_t len = 0;

  memcpy(&len, rbuf, sizeof(len));

  if (len > k_max_msg) {
    msg("message too long");
    return -1;
  }

  err = read_full(connfd, rbuf + 4, len);
  if (err) {
    msg("read() error");
    return -1;
  }

  printf("client says: %.*s\n", len, rbuf + 4);

  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)] = {};
  uint32_t reply_len = (uint32_t)strlen(reply);

  memcpy(wbuf, &reply_len, 4);
  memcpy(wbuf + 4, reply, reply_len);

  return write_full(connfd, wbuf, 4 + reply_len);
}

int main() {

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    throwsyserror("socket() failed");
  }

  int value = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(1212);
  addr.sin_addr.s_addr = INADDR_ANY;

  int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

  if (rv) {
    throwsyserror("bind() failed");
  }

  rv = listen(fd, SOMAXCONN);

  if (rv) {
    throwsyserror("listen() failed");
  }

  while (true) {
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);

    if (connfd < 0) {
      continue;
    }

    while (true) {
      int32_t err = one_request(connfd);

      if (err < 0) {
        break;
      }
    }
    close(connfd);
  }
}
