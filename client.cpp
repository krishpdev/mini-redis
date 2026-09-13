#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <iostream>
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

    assert(static_cast<size_t>(n) <= count);

    count -= static_cast<size_t>(n);
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

    assert(static_cast<size_t>(n) <= count);

    count -= static_cast<size_t>(n);
    rbuf += n;
  }

  return 0;
}

static int32_t query(int fd, const char *text) {
  uint32_t len = static_cast<uint32_t>(strlen(text));
  if (len > k_max_msg) {
    fprintf(stderr, "error: message too long\n");
    return -1;
  }

  char wbuf[4 + k_max_msg] = {};
  memcpy(wbuf, &len, sizeof(len));
  memcpy(wbuf + 4, text, len);

  int32_t err = write_full(fd, wbuf, 4 + len);
  if (err) {
    fprintf(stderr, "error: write_full failed\n");
    return err;
  }

  char rbuf[4 + k_max_msg] = {};
  int32_t err2 = read_full(fd, rbuf, 4);
  if (err2) {
    fprintf(stderr, "error: read length prefix failed\n");
    return err2;
  }

  memcpy(&len, rbuf, sizeof(len));

  if (len > k_max_msg) {
    fprintf(stderr, "error: response length %u exceeds max\n", len);
    return -1;
  }

  int32_t err3 = read_full(fd, rbuf + 4, len);
  if (err3) {
    fprintf(stderr, "error: read response payload failed\n");
    return err3;
  }

  printf("server says: %.*s\n", (int)len, rbuf + 4);
  return 0;
}

int main(int argc, char **argv) {
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
    close(fd);
    throw std::system_error(errno, std::system_category(), "connect() failed");
  }

  // CLI Arguments mode: e.g. ./client "hello" "world"
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      int32_t err = query(fd, argv[i]);
      if (err) {
        break;
      }
    }
    close(fd);
    return 0;
  }

  // Interactive REPL mode: type lines to send
  std::cout << "Connected to server on port 1212. Type messages (or 'quit'/'exit' to stop):\n";
  std::string line;
  while (true) {
    std::cout << "> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }
    if (line == "quit" || line == "exit") {
      break;
    }
    if (line.empty()) {
      continue;
    }
    int32_t err = query(fd, line.c_str());
    if (err) {
      std::cerr << "Server disconnected or error occurred.\n";
      break;
    }
  }

  close(fd);
  return 0;
}
