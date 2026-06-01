/* Helpers for poll error tests */
#ifndef POLL_TEST_HELPERS_H
#define POLL_TEST_HELPERS_H

#include "../src/poll.h"

Poll Poll_make_invalid_() {
  Poll p;
#ifdef CARP_USE_KQUEUE
  p.kq = -1;
#elif defined(CARP_USE_EPOLL)
  p.epfd = -1;
#endif
  return p;
}

#endif
