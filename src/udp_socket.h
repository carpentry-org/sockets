#ifndef CARP_UDP_SOCKET_H
#define CARP_UDP_SOCKET_H

#include "common.h"

typedef struct {
  int fd;
  struct sockaddr_storage bound;
  socklen_t bound_len;
} UdpSocket;

UdpSocket UdpSocket_bind_(String* host, int port) {
  UdpSocket u;
  u.fd = -1;
  u.bound_len = 0;
  memset(&u.bound, 0, sizeof(u.bound));

  if (resolve_address(*host, port, SOCK_DGRAM, &u.bound, &u.bound_len) != 0)
    return u;

  u.fd = socket(u.bound.ss_family, SOCK_DGRAM, 0);
  if (u.fd < 0) return u;

  if (bind(u.fd, (struct sockaddr*)&u.bound, u.bound_len) < 0) {
    close(u.fd);
    u.fd = -1;
    return u;
  }

  getsockname(u.fd, (struct sockaddr*)&u.bound, &u.bound_len);
  return u;
}

int UdpSocket_fd_(UdpSocket* u) { return u->fd; }

int UdpSocket_send_MINUS_to_(UdpSocket* u, String* host, int port, Array* data) {
  struct sockaddr_storage dest;
  socklen_t dest_len;
  if (resolve_address(*host, port, SOCK_DGRAM, &dest, &dest_len) != 0) return -1;
  return (int)sendto(u->fd, data->data, data->len, 0,
                     (struct sockaddr*)&dest, dest_len);
}

/* Returns bytes received, fills sender address/port. -1 on error. */
int UdpSocket_recv_(UdpSocket* u, Array* buf, String* sender_out, int* port_out) {
  if ((int)buf->capacity < SOCK_BUF_SIZE) {
    buf->data = CARP_REALLOC(buf->data, SOCK_BUF_SIZE);
    buf->capacity = SOCK_BUF_SIZE;
  }
  struct sockaddr_storage from;
  socklen_t from_len = sizeof(from);
  ssize_t n = recvfrom(u->fd, buf->data, buf->capacity, 0,
                       (struct sockaddr*)&from, &from_len);
  if (n < 0) return -1;
  buf->len = (int)n;

  /* Write sender address */
  CARP_FREE(*sender_out);
  *sender_out = sockaddr_to_string(&from, from_len);
  *port_out = sockaddr_port(&from);
  return (int)n;
}

void UdpSocket_close(UdpSocket u) {
  if (u.fd >= 0) close(u.fd);
}

void UdpSocket_set_MINUS_timeout(UdpSocket* u, int seconds) {
  struct timeval tv = { .tv_sec = seconds, .tv_usec = 0 };
  setsockopt(u->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(u->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

UdpSocket UdpSocket_copy(UdpSocket* u) {
  UdpSocket c;
  c.fd = u->fd;
  memcpy(&c.bound, &u->bound, sizeof(c.bound));
  c.bound_len = u->bound_len;
  return c;
}

#endif
