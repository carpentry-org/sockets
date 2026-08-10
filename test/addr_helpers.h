/* Helpers for the address-resolution tests */
#ifndef ADDR_TEST_HELPERS_H
#define ADDR_TEST_HELPERS_H

#include "../src/common.h"

static int addr_test_lookup(int socktype, int* has4, int* has6) {
  struct addrinfo hints, *result, *rp;
  *has4 = 0;
  *has6 = 0;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = socktype;
  hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

  if (getaddrinfo("localhost", "0", &hints, &result) != 0) return -1;
  if (!result) return -1;

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if (rp->ai_family == AF_INET) *has4 = 1;
    else if (rp->ai_family == AF_INET6) *has6 = 1;
  }
  freeaddrinfo(result);
  return 0;
}

int AddrTest_dgram_MINUS_ipv4_() {
  int has4, has6;
  if (addr_test_lookup(SOCK_DGRAM, &has4, &has6) != 0) return 0;
  return has4;
}

int AddrTest_dgram_MINUS_ipv6_() {
  int has4, has6;
  if (addr_test_lookup(SOCK_DGRAM, &has4, &has6) != 0) return 0;
  return has6;
}

int AddrTest_stream_MINUS_families_() {
  int has4, has6;
  if (addr_test_lookup(SOCK_STREAM, &has4, &has6) != 0) return 0;
  return has4 + has6;
}

/* Occupies the first candidate getaddrinfo returns for localhost, without
   SO_REUSEADDR/SO_REUSEPORT so that a rebind gets EADDRINUSE. Returns the
   listening fd, -1 if the setup failed, or -2 if the address turned out to
   be rebindable anyway. */
int AddrTest_occupy_MINUS_first_() {
  struct addrinfo hints, *result;
  int fd, probe, opt = 1, rebindable;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

  if (getaddrinfo("localhost", "0", &hints, &result) != 0) return -1;
  if (!result) return -1;

  fd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (fd < 0) {
    freeaddrinfo(result);
    return -1;
  }
  if (bind(fd, result->ai_addr, result->ai_addrlen) < 0 || listen(fd, 8) < 0) {
    close(fd);
    freeaddrinfo(result);
    return -1;
  }

  probe = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (probe < 0) {
    close(fd);
    freeaddrinfo(result);
    return -1;
  }
  setsockopt(probe, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
  setsockopt(probe, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
  {
    struct sockaddr_storage taken;
    socklen_t taken_len = sizeof(taken);
    getsockname(fd, (struct sockaddr*)&taken, &taken_len);
    rebindable = bind(probe, (struct sockaddr*)&taken, taken_len) == 0;
  }
  close(probe);
  freeaddrinfo(result);

  if (rebindable) {
    close(fd);
    return -2;
  }
  return fd;
}

int AddrTest_occupied_MINUS_port_(int fd) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof(addr);
  if (getsockname(fd, (struct sockaddr*)&addr, &len) < 0) return -1;
  return sockaddr_port(&addr);
}

void AddrTest_release_(int fd) {
  if (fd >= 0) close(fd);
}

#endif
