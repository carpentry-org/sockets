#ifndef CARP_UNIX_BUFIO_H
#define CARP_UNIX_BUFIO_H

#include "unix_stream.h"
#include "../../bufio/src/bufio.h"

/* Adapters that bridge UnixStream to BufReader's function pointer interface */

static int unix_stream_read_adapter(void* inner, char* buf, int len) {
  UnixStream* s = (UnixStream*)inner;
  return (int)read(s->fd, buf, len);
}

static int unix_stream_write_adapter(void* inner, const char* buf, int len) {
  return (int)send_all(((UnixStream*)inner)->fd, buf, len);
}

static void unix_stream_close_adapter(void* inner) {
  UnixStream* s = (UnixStream*)inner;
  if (s->fd >= 0) close(s->fd);
  CARP_FREE(s);
}

BufReader UnixStream_buffered(UnixStream s) {
  /* Move the UnixStream onto the heap so BufReader owns it */
  UnixStream* heap = CARP_MALLOC(sizeof(UnixStream));
  *heap = s;
  return BufReader_create_((void*)heap,
                           unix_stream_read_adapter,
                           unix_stream_write_adapter,
                           unix_stream_close_adapter);
}

#endif
