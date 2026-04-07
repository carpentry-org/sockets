#ifndef CARP_TCP_LISTENER_H
#define CARP_TCP_LISTENER_H

#include "tcp_stream.h"

typedef struct {
  int fd;
  struct sockaddr_storage addr;
  socklen_t addr_len;
} TcpListener;

TcpListener TcpListener_bind_(String* host, int port) {
  TcpListener l;
  l.fd = -1;
  l.addr_len = 0;
  memset(&l.addr, 0, sizeof(l.addr));

  if (resolve_address(*host, port, SOCK_STREAM, &l.addr, &l.addr_len) != 0)
    return l;

  l.fd = socket(l.addr.ss_family, SOCK_STREAM, 0);
  if (l.fd < 0) return l;

  int opt = 1;
  setsockopt(l.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
  setsockopt(l.fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

  if (bind(l.fd, (struct sockaddr*)&l.addr, l.addr_len) < 0) {
    close(l.fd);
    l.fd = -1;
    return l;
  }

  if (listen(l.fd, 128) < 0) {
    close(l.fd);
    l.fd = -1;
    return l;
  }

  /* Update addr to reflect the actual bound address (e.g., port 0 assignment) */
  getsockname(l.fd, (struct sockaddr*)&l.addr, &l.addr_len);
  return l;
}

int TcpListener_fd_(TcpListener* l) { return l->fd; }

TcpStream TcpListener_accept_(TcpListener* l) {
  TcpStream s;
  s.peer_len = sizeof(s.peer);
  memset(&s.peer, 0, sizeof(s.peer));
  s.fd = accept(l->fd, (struct sockaddr*)&s.peer, &s.peer_len);
  return s;
}

void TcpListener_close(TcpListener l) {
  if (l.fd >= 0) close(l.fd);
}

String TcpListener_local_MINUS_address(TcpListener* l) {
  return sockaddr_to_string(&l->addr, l->addr_len);
}

int TcpListener_local_MINUS_port(TcpListener* l) {
  return sockaddr_port(&l->addr);
}

void TcpListener_set_MINUS_reuseaddr(TcpListener* l, int enable) {
  setsockopt(l->fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
}

TcpListener TcpListener_copy(TcpListener* l) {
  TcpListener c;
  c.fd = l->fd;
  memcpy(&c.addr, &l->addr, sizeof(c.addr));
  c.addr_len = l->addr_len;
  return c;
}

#endif
