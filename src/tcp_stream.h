#ifndef CARP_TCP_STREAM_H
#define CARP_TCP_STREAM_H

#include "common.h"

typedef struct {
  int fd;
  struct sockaddr_storage peer;
  socklen_t peer_len;
} TcpStream;

TcpStream TcpStream_connect_(String* host, int port) {
  TcpStream s;
  s.fd = -1;
  s.peer_len = 0;
  memset(&s.peer, 0, sizeof(s.peer));

  struct addrinfo hints, *result, *rp;
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;

  if (getaddrinfo(*host, port_str, &hints, &result) != 0) return s;

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      s.fd = fd;
      memcpy(&s.peer, rp->ai_addr, rp->ai_addrlen);
      s.peer_len = rp->ai_addrlen;
      break;
    }
    close(fd);
  }
  freeaddrinfo(result);
  return s;
}

int TcpStream_fd_(TcpStream* s) { return s->fd; }

int TcpStream_send_(TcpStream* s, String* msg) {
  return (int)send_all(s->fd, *msg, strlen(*msg));
}

int TcpStream_send_MINUS_bytes_(TcpStream* s, Array* data) {
  return (int)send_all(s->fd, (const char*)data->data, data->len);
}

int TcpStream_send_MINUS_len_(TcpStream* s, String* msg, int len) {
  return (int)send_all(s->fd, *msg, (size_t)len);
}

String TcpStream_read_(TcpStream* s) {
  String buf = CARP_MALLOC(SOCK_BUF_SIZE + 1);
  ssize_t r = read(s->fd, buf, SOCK_BUF_SIZE);
  if (r < 0) {
    buf[0] = '\0';
    return buf;
  }
  buf[r] = '\0';
  return buf;
}

Array TcpStream_read_MINUS_bytes_(TcpStream* s) {
  Array buf;
  buf.capacity = SOCK_BUF_SIZE;
  buf.data = CARP_MALLOC(SOCK_BUF_SIZE);
  ssize_t r = read(s->fd, buf.data, SOCK_BUF_SIZE);
  buf.len = r < 0 ? 0 : (int)r;
  return buf;
}

int TcpStream_read_MINUS_append_(TcpStream* s, Array* buf) {
  if ((int)(buf->capacity - buf->len) < SOCK_BUF_SIZE) {
    int new_cap = (buf->len + SOCK_BUF_SIZE) * 2;
    buf->data = CARP_REALLOC(buf->data, new_cap);
    buf->capacity = new_cap;
  }
  ssize_t r = read(s->fd, (char*)buf->data + buf->len, SOCK_BUF_SIZE);
  if (r > 0) buf->len += (int)r;
  return (int)r;
}

/* Put the socket into non-blocking mode. After this call, send and read
 * operations will return EAGAIN/EWOULDBLOCK instead of blocking. */
void TcpStream_set_MINUS_nonblocking(TcpStream* s) {
  int flags = fcntl(s->fd, F_GETFL, 0);
  if (flags >= 0) fcntl(s->fd, F_SETFL, flags | O_NONBLOCK);
}

/* Non-blocking send. Sends as many bytes as the kernel will accept right
 * now, starting at `offset` into `data`.
 *
 * Returns:
 *    >= 0  number of bytes actually sent (may be 0 if would-block)
 *    -1    real error (errno is set)
 */
int TcpStream_send_MINUS_nb_(TcpStream* s, Array* data, int offset) {
  if (offset >= data->len) return 0;
  ssize_t n = send(s->fd, (char*)data->data + offset,
                   (size_t)(data->len - offset), 0);
  if (n >= 0) return (int)n;
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
  return -1;
}

/* Non-blocking append-read. Reads whatever the kernel has buffered into
 * `buf`, growing the array if needed.
 *
 * Returns:
 *    > 0  number of bytes appended
 *      0  peer closed cleanly (EOF)
 *    -1   real error
 *    -2   would block (EAGAIN/EWOULDBLOCK), retry on next readable event
 */
int TcpStream_read_MINUS_append_MINUS_nb_(TcpStream* s, Array* buf) {
  if ((int)(buf->capacity - buf->len) < SOCK_BUF_SIZE) {
    int new_cap = (buf->len + SOCK_BUF_SIZE) * 2;
    buf->data = CARP_REALLOC(buf->data, new_cap);
    buf->capacity = new_cap;
  }
  ssize_t r = read(s->fd, (char*)buf->data + buf->len, SOCK_BUF_SIZE);
  if (r > 0) { buf->len += (int)r; return (int)r; }
  if (r == 0) return 0;
  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return -2;
  return -1;
}

void TcpStream_clear_MINUS_buf(Array* buf) {
  buf->len = 0;
}

void TcpStream_close_MINUS_ref(TcpStream* s) {
  if (s->fd >= 0) { close(s->fd); s->fd = -1; }
}

void TcpStream_close(TcpStream s) {
  if (s.fd >= 0) close(s.fd);
}

void TcpStream_shutdown_(TcpStream* s, int how) {
  shutdown(s->fd, how);
}

void TcpStream_shutdown_MINUS_read(TcpStream* s) {
  shutdown(s->fd, SHUT_RD);
}

void TcpStream_shutdown_MINUS_write(TcpStream* s) {
  shutdown(s->fd, SHUT_WR);
}

void TcpStream_set_MINUS_nodelay(TcpStream* s) {
  int flag = 1;
  setsockopt(s->fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

void TcpStream_set_MINUS_timeout(TcpStream* s, int seconds) {
  struct timeval tv = { .tv_sec = seconds, .tv_usec = 0 };
  setsockopt(s->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(s->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

void TcpStream_set_MINUS_read_MINUS_timeout(TcpStream* s, int seconds) {
  struct timeval tv = { .tv_sec = seconds, .tv_usec = 0 };
  setsockopt(s->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

String TcpStream_peer_MINUS_address(TcpStream* s) {
  return sockaddr_to_string(&s->peer, s->peer_len);
}

int TcpStream_peer_MINUS_port(TcpStream* s) {
  return sockaddr_port(&s->peer);
}

void TcpStream_set_MINUS_keepalive(TcpStream* s, int enable) {
  setsockopt(s->fd, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable));
}

void TcpStream_set_MINUS_linger(TcpStream* s, int seconds) {
  struct linger l;
  l.l_onoff = seconds >= 0 ? 1 : 0;
  l.l_linger = seconds >= 0 ? seconds : 0;
  setsockopt(s->fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
}

void TcpStream_set_MINUS_send_MINUS_buffer(TcpStream* s, int size) {
  setsockopt(s->fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
}

void TcpStream_set_MINUS_recv_MINUS_buffer(TcpStream* s, int size) {
  setsockopt(s->fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
}

TcpStream TcpStream_connect_MINUS_timeout_(String* host, int port, int timeout_sec) {
  TcpStream s;
  s.fd = -1;
  s.peer_len = 0;
  memset(&s.peer, 0, sizeof(s.peer));

  struct addrinfo hints, *result, *rp;
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_ADDRCONFIG;
  if (getaddrinfo(*host, port_str, &hints, &result) != 0) return s;

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0) continue;

    /* Set non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int ret = connect(fd, rp->ai_addr, rp->ai_addrlen);
    if (ret == 0) {
      /* Connected immediately */
      fcntl(fd, F_SETFL, flags); /* restore blocking */
      s.fd = fd;
      memcpy(&s.peer, rp->ai_addr, rp->ai_addrlen);
      s.peer_len = rp->ai_addrlen;
      break;
    }
    if (errno != EINPROGRESS) { close(fd); continue; }

    /* Wait with timeout */
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(fd, &wset);
    struct timeval tv = { .tv_sec = timeout_sec, .tv_usec = 0 };
    ret = select(fd + 1, NULL, &wset, NULL, &tv);
    if (ret <= 0) { close(fd); continue; } /* timeout or error */

    /* Check for connection error */
    int err = 0;
    socklen_t errlen = sizeof(err);
    getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
    if (err != 0) { close(fd); errno = err; continue; }

    fcntl(fd, F_SETFL, flags); /* restore blocking */
    s.fd = fd;
    memcpy(&s.peer, rp->ai_addr, rp->ai_addrlen);
    s.peer_len = rp->ai_addrlen;
    break;
  }
  freeaddrinfo(result);
  return s;
}

TcpStream TcpStream_copy(TcpStream* s) {
  TcpStream c;
  c.fd = s->fd;
  memcpy(&c.peer, &s->peer, sizeof(c.peer));
  c.peer_len = s->peer_len;
  return c;
}

#endif
