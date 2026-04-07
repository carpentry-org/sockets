#ifndef CARP_UNIX_LISTENER_H
#define CARP_UNIX_LISTENER_H

#include "unix_stream.h"

typedef struct {
  int fd;
  struct sockaddr_un addr;
} UnixListener;

UnixListener UnixListener_bind_(String* path) {
  UnixListener l;
  l.fd = -1;
  memset(&l.addr, 0, sizeof(l.addr));

  /* Remove existing socket file if present */
  unlink(*path);

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return l;

  l.addr.sun_family = AF_UNIX;
  strncpy(l.addr.sun_path, *path, sizeof(l.addr.sun_path) - 1);

  if (bind(fd, (struct sockaddr*)&l.addr, sizeof(l.addr)) < 0) {
    close(fd);
    return l;
  }

  if (listen(fd, 128) < 0) {
    close(fd);
    l.fd = -1;
    return l;
  }

  l.fd = fd;
  return l;
}

int UnixListener_fd_(UnixListener* l) { return l->fd; }

UnixStream UnixListener_accept_(UnixListener* l) {
  UnixStream s;
  memset(&s.peer, 0, sizeof(s.peer));
  socklen_t len = sizeof(s.peer);
  s.fd = accept(l->fd, (struct sockaddr*)&s.peer, &len);
  return s;
}

void UnixListener_close(UnixListener l) {
  if (l.fd >= 0) {
    close(l.fd);
    unlink(l.addr.sun_path);
  }
}

String UnixListener_path(UnixListener* l) {
  const char* p = l->addr.sun_path;
  size_t len = strlen(p);
  String str = CARP_MALLOC(len + 1);
  memcpy(str, p, len + 1);
  return str;
}

UnixListener UnixListener_copy(UnixListener* l) {
  UnixListener c;
  c.fd = l->fd;
  memcpy(&c.addr, &l->addr, sizeof(c.addr));
  return c;
}

#endif
