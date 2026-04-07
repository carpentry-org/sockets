#ifndef CARP_SOCK_COMMON_H
#define CARP_SOCK_COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCK_BUF_SIZE 4096

__attribute__((unused))
static String sock_error_string() {
  const char* msg = strerror(errno);
  size_t len = strlen(msg);
  String s = CARP_MALLOC(len + 1);
  memcpy(s, msg, len + 1);
  return s;
}

__attribute__((unused))
static String sockaddr_to_string(struct sockaddr_storage* addr, socklen_t len) {
  char buf[INET6_ADDRSTRLEN];
  buf[0] = '\0';
  if (addr->ss_family == AF_INET) {
    struct sockaddr_in* v4 = (struct sockaddr_in*)addr;
    inet_ntop(AF_INET, &v4->sin_addr, buf, sizeof(buf));
  } else if (addr->ss_family == AF_INET6) {
    struct sockaddr_in6* v6 = (struct sockaddr_in6*)addr;
    inet_ntop(AF_INET6, &v6->sin6_addr, buf, sizeof(buf));
  }
  size_t slen = strlen(buf);
  String s = CARP_MALLOC(slen + 1);
  memcpy(s, buf, slen + 1);
  return s;
}

__attribute__((unused))
static int sockaddr_port(struct sockaddr_storage* addr) {
  if (addr->ss_family == AF_INET) {
    return ntohs(((struct sockaddr_in*)addr)->sin_port);
  } else if (addr->ss_family == AF_INET6) {
    return ntohs(((struct sockaddr_in6*)addr)->sin6_port);
  }
  return 0;
}

/* Resolves host:port to a sockaddr_storage. Returns 0 on success, -1 on error. */
__attribute__((unused))
static int resolve_address(const char* host, int port, int socktype,
                           struct sockaddr_storage* out, socklen_t* out_len) {
  struct addrinfo hints, *result;
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = socktype;
  hints.ai_flags = AI_ADDRCONFIG;

  if (getaddrinfo(host, port_str, &hints, &result) != 0) return -1;
  if (!result) return -1;

  memcpy(out, result->ai_addr, result->ai_addrlen);
  *out_len = result->ai_addrlen;
  freeaddrinfo(result);
  return 0;
}

/* Send all bytes, looping on partial writes. Returns total sent or -1. */
__attribute__((unused))
static ssize_t send_all(int fd, const char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(fd, data + sent, len - sent, 0);
    if (n <= 0) return -1;
    sent += n;
  }
  return (ssize_t)sent;
}

#endif
