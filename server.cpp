#include <arpa/inet.h>
#include <assert.h>
#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
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

static void fd_set_nb(int fd) {
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static void buffer_insert(std::vector<uint8_t> &buffer, const uint8_t *data,
                          size_t len) {
  buffer.insert(buffer.end(), data, data + len);
}

static void buffer_erase(std::vector<uint8_t> &buffer, size_t offset,
                         size_t len) {
  buffer.erase(buffer.begin() + offset, buffer.begin() + offset + len);
}

static bool try_one_request(Connection *connection) {

  if (connection->read_buffer.size() < 4) {
    return false;
  }

  uint32_t len = 0;

  mempcpy(&len, connection->read_buffer.data(), 4);

  if (len > k_max_msg) {
    connection->want_close = true;
    return false;
  }

  if (4 + len > connection->read_buffer.size()) {
    return false;
  }

  const uint8_t *request = connection->read_buffer.data() + 4;

  buffer_insert(connection->write_buffer, (const uint8_t *)&len, 4);
  buffer_insert(connection->write_buffer, request, len);

  buffer_erase(connection->read_buffer, 0, 4 + len);

  return true;
}

static Connection *handle_accept(int fd) {
  struct sockaddr_in client_addr = {};

  socklen_t addrlen = sizeof(client_addr);

  int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);

  if (connfd < 0) {
    return nullptr;
  }

  fd_set_nb(connfd);

  Connection *connection = new Connection();
  connection->fd = connfd;
  connection->want_read = true;
  return connection;
}

static void handle_read(Connection *connection) {
  char buf[1024] = {};
  ssize_t n = read(connection->fd, buf, sizeof(buf));

  if (n <= 0) {
    connection->want_close = true;
    return;
  }

  fprintf(stderr, "client says: %.*s\n", (int)n, buf);

  buffer_insert(connection->read_buffer, buf, (size_t)n);

  try_one_request(connection);
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

    int rv = poll(pollfds.data(), (nfds_t)pollfds.size(), -1);

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

    for (size_t i = 1; i < pollfds.size(); i++) {
      uint32_t ready = pollfds[i].revents;

      Connection *connection = fd_connections[pollfds[i].fd];

      if (ready & POLLIN) {
        handle_read(connection);
      }

      if (ready & POLLOUT) {
        handle_write(connection);
      }

      if (ready & POLLERR || connection->want_close) {
        (void)close(connection->fd);
        fd_connections[connection->fd] = nullptr;
        delete connection;
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
