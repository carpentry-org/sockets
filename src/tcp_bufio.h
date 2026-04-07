#ifndef CARP_TCP_BUFIO_H
#define CARP_TCP_BUFIO_H

#include "tcp_stream.h"
#include "../../bufio/src/bufio.h"

/* Adapters that bridge TcpStream to BufReader's function pointer interface */

static int tcp_stream_read_adapter(void* inner, char* buf, int len) {
  TcpStream* s = (TcpStream*)inner;
  return (int)read(s->fd, buf, len);
}

static int tcp_stream_write_adapter(void* inner, const char* buf, int len) {
  return (int)send_all(((TcpStream*)inner)->fd, buf, len);
}

static void tcp_stream_close_adapter(void* inner) {
  TcpStream* s = (TcpStream*)inner;
  if (s->fd >= 0) close(s->fd);
  CARP_FREE(s);
}

BufReader TcpStream_buffered(TcpStream s) {
  /* Move the TcpStream onto the heap so BufReader owns it */
  TcpStream* heap = CARP_MALLOC(sizeof(TcpStream));
  *heap = s;
  return BufReader_create_((void*)heap,
                           tcp_stream_read_adapter,
                           tcp_stream_write_adapter,
                           tcp_stream_close_adapter);
}

#endif
