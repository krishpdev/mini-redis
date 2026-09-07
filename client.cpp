#include <arpa/inet.h>
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

  std::string message =
      "Hello! this is a test of the client & server communication via sockets";

  write(fd, message.data(), message.size());

  std::string rmessage(64, '\0');
  ssize_t n = read(fd, rmessage.data(), rmessage.size() - 1);

  if (n < 0) {
    throw std::system_error(errno, std::system_category(), "read() failed");
  }

  close(fd);
}
