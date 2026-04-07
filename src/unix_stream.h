#ifndef CARP_UNIX_STREAM_H
#define CARP_UNIX_STREAM_H

#include "common.h"
#include <sys/un.h>

typedef struct {
  int fd;
  struct sockaddr_un peer;
} UnixStream;

UnixStream UnixStream_connect_(String* path) {
  UnixStream s;
  s.fd = -1;
  memset(&s.peer, 0, sizeof(s.peer));

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return s;

  s.peer.sun_family = AF_UNIX;
  strncpy(s.peer.sun_path, *path, sizeof(s.peer.sun_path) - 1);

  if (connect(fd, (struct sockaddr*)&s.peer, sizeof(s.peer)) < 0) {
    close(fd);
    return s;
  }

  s.fd = fd;
  return s;
}

int UnixStream_fd_(UnixStream* s) { return s->fd; }

int UnixStream_send_(UnixStream* s, String* msg) {
  return (int)send_all(s->fd, *msg, strlen(*msg));
}

int UnixStream_send_MINUS_bytes_(UnixStream* s, Array* data) {
  return (int)send_all(s->fd, (const char*)data->data, data->len);
}

String UnixStream_read_(UnixStream* s) {
  String buf = CARP_MALLOC(SOCK_BUF_SIZE + 1);
  ssize_t r = read(s->fd, buf, SOCK_BUF_SIZE);
  if (r < 0) {
    buf[0] = '\0';
    return buf;
  }
  buf[r] = '\0';
  return buf;
}

Array UnixStream_read_MINUS_bytes_(UnixStream* s) {
  Array buf;
  buf.capacity = SOCK_BUF_SIZE;
  buf.data = CARP_MALLOC(SOCK_BUF_SIZE);
  ssize_t r = read(s->fd, buf.data, SOCK_BUF_SIZE);
  buf.len = r < 0 ? 0 : (int)r;
  return buf;
}

int UnixStream_read_MINUS_append_(UnixStream* s, Array* buf) {
  if ((int)(buf->capacity - buf->len) < SOCK_BUF_SIZE) {
    int new_cap = (buf->len + SOCK_BUF_SIZE) * 2;
    buf->data = CARP_REALLOC(buf->data, new_cap);
    buf->capacity = new_cap;
  }
  ssize_t r = read(s->fd, (char*)buf->data + buf->len, SOCK_BUF_SIZE);
  if (r > 0) buf->len += (int)r;
  return (int)r;
}

void UnixStream_close(UnixStream s) {
  if (s.fd >= 0) close(s.fd);
}

void UnixStream_shutdown_(UnixStream* s, int how) {
  shutdown(s->fd, how);
}

void UnixStream_shutdown_MINUS_read(UnixStream* s) {
  shutdown(s->fd, SHUT_RD);
}

void UnixStream_shutdown_MINUS_write(UnixStream* s) {
  shutdown(s->fd, SHUT_WR);
}

void UnixStream_set_MINUS_timeout(UnixStream* s, int seconds) {
  struct timeval tv = { .tv_sec = seconds, .tv_usec = 0 };
  setsockopt(s->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

String UnixStream_peer_MINUS_path(UnixStream* s) {
  const char* p = s->peer.sun_path;
  size_t len = strlen(p);
  String str = CARP_MALLOC(len + 1);
  memcpy(str, p, len + 1);
  return str;
}

UnixStream UnixStream_copy(UnixStream* s) {
  UnixStream c;
  c.fd = s->fd;
  memcpy(&c.peer, &s->peer, sizeof(c.peer));
  return c;
}

#endif
