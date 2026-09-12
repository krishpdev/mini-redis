#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

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

struct Connection {
  int fd = 1;

  bool want_read = false;
  bool want_write = false;
  bool want_close = false;

  std::vector<uint8_t> read_buffer;
  std::vector<uint8_t> write_buffer;
};

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

  std::vector<Connection *> fd_connections;

  std::vector<struct pollfd> pollfds;

  while (true) {

    pollfds.clear();

    struct pollfd pfd = {fd, POLLIN, 0};

    pollfds.push_back(pfd);

    for (Connection *connection : fd_connections) {
      if (!connection) {
        continue;
      }

      struct pollfd pfd = {connection->fd, POLLERR, 0};

      if (connection->want_read) {
        pfd.events |= POLLIN;
      }

      if (connection->want_write) {
        pfd.events |= POLLOUT;
      }

      pollfds.push_back(pfd);
    }

    int rv = poll(pollfds.data(), pollfds.size(), -1);

    if (rv < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        throwsyserror("poll() failed");
      }
    }

    if (pollfds[0].revents) {
      if (Connection *connection = handle_accept(fd)) {
        if (fd_connections.size() <= (size_t)connection->fd) {
          fd_connections.resize(connection->fd + 1);
        }
        fd_connections[connection->fd] = connection;
      }
    }
  }
  //    struct sockaddr_in client_addr = {};
  //    socklen_t addrlen = sizeof(client_addr);
  //    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
  //
  //    fd_set want_read;
  //    fd_set want_write;
  //
  //    can_read, can_write = wait_for_io(connfd, &want_read, &want_write);
}
