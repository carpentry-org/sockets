#ifndef CARP_POLL_H
#define CARP_POLL_H

#include "common.h"

/* --------------------------------------------------------------------------
 * PollEvent — returned by Poll.wait, one per ready notification
 * -------------------------------------------------------------------------- */

typedef struct {
  int fd;
  bool readable;
  bool writable;
  bool error;
} PollEvent;

bool PollEvent_readable(PollEvent* e) { return e->readable; }
bool PollEvent_writable(PollEvent* e) { return e->writable; }
bool PollEvent_error_QMARK_(PollEvent* e) { return e->error; }
int PollEvent_fd(PollEvent* e) { return e->fd; }

PollEvent PollEvent_copy(PollEvent* e) {
  PollEvent c = *e;
  return c;
}

/* --------------------------------------------------------------------------
 * Interest flags
 * -------------------------------------------------------------------------- */

#define POLL_READ  1
#define POLL_WRITE 2

/* --------------------------------------------------------------------------
 * Platform-specific Poll implementation
 * -------------------------------------------------------------------------- */

#if defined(CARP_FORCE_POLL)
#define CARP_USE_POLL 1
#include <poll.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define CARP_USE_KQUEUE 1
#include <sys/event.h>
#elif defined(__linux__)
#define CARP_USE_EPOLL 1
#include <sys/epoll.h>
#else
#define CARP_USE_POLL 1
#include <poll.h>
#endif

/* Max events per wait call */
#define POLL_MAX_EVENTS 256

#ifdef CARP_USE_KQUEUE

typedef struct {
  int kq;
} Poll;

Poll Poll_create_() {
  Poll p;
  p.kq = kqueue();
  return p;
}

int Poll_fd_(Poll* p) { return p->kq; }

int Poll_add_(Poll* p, int fd, int interest) {
  struct kevent changes[2];
  int nchanges = 0;
  if (interest & POLL_READ) {
    EV_SET(&changes[nchanges], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    nchanges++;
  }
  if (interest & POLL_WRITE) {
    EV_SET(&changes[nchanges], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    nchanges++;
  }
  return kevent(p->kq, changes, nchanges, NULL, 0, NULL) < 0 ? -1 : 0;
}

/* Delete one filter, tolerating ENOENT.
 *
 * kevent() stops processing the changelist at the first entry that fails, so
 * batching both deletes means an unarmed filter (ENOENT) prevents the other
 * from ever being removed. A leaked EVFILT_WRITE then reports the socket
 * writable forever, which an event loop sees as a write with no pending
 * buffer. Issue the deletes separately so each is applied independently. */
static void poll_kq_del(int kq, int fd, int16_t filter) {
  struct kevent ch;
  EV_SET(&ch, fd, filter, EV_DELETE, 0, 0, NULL);
  kevent(kq, &ch, 1, NULL, 0, NULL); /* ENOENT is expected and harmless */
}

int Poll_modify_(Poll* p, int fd, int interest) {
  /* Remove both, then add desired */
  poll_kq_del(p->kq, fd, EVFILT_READ);
  poll_kq_del(p->kq, fd, EVFILT_WRITE);
  return Poll_add_(p, fd, interest);
}

int Poll_remove_(Poll* p, int fd) {
  /* Separate calls, for the same reason as Poll_modify_ above. */
  poll_kq_del(p->kq, fd, EVFILT_READ);
  poll_kq_del(p->kq, fd, EVFILT_WRITE);
  return 0;
}

Array Poll_wait_(Poll* p, int timeout_ms) {
  struct kevent events[POLL_MAX_EVENTS];
  struct timespec ts, *tsp = NULL;
  if (timeout_ms >= 0) {
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
    tsp = &ts;
  }

  int n = kevent(p->kq, NULL, 0, events, POLL_MAX_EVENTS, tsp);

  Array result;
  if (n < 0) {
    result.len = 0; result.capacity = 0; result.data = NULL;
    return result;
  }

  /* Coalesce events for the same fd */
  /* First pass: collect unique fds and their combined flags */
  int* fds = NULL;
  int* flags = NULL;
  int unique = 0;

  if (n > 0) {
    fds = (int*)CARP_MALLOC(n * sizeof(int));
    if (!fds) {
      result.len = 0; result.capacity = 0; result.data = NULL;
      return result;
    }
    flags = (int*)CARP_MALLOC(n * sizeof(int));
    if (!flags) {
      CARP_FREE(fds);
      result.len = 0; result.capacity = 0; result.data = NULL;
      return result;
    }

    for (int i = 0; i < n; i++) {
      int fd = (int)events[i].ident;
      int flag = 0;
      if (events[i].filter == EVFILT_READ) flag |= POLL_READ;
      if (events[i].filter == EVFILT_WRITE) flag |= POLL_WRITE;
      if (events[i].flags & EV_ERROR) flag |= 4; /* error flag */

      /* Find existing entry for this fd */
      int found = -1;
      for (int j = 0; j < unique; j++) {
        if (fds[j] == fd) { found = j; break; }
      }
      if (found >= 0) {
        flags[found] |= flag;
      } else {
        fds[unique] = fd;
        flags[unique] = flag;
        unique++;
      }
    }
  }

  result.len = unique;
  result.capacity = unique > 0 ? unique : 1;
  result.data = CARP_MALLOC(result.capacity * sizeof(PollEvent));
  if (!result.data) {
    if (fds) CARP_FREE(fds);
    if (flags) CARP_FREE(flags);
    result.len = 0; result.capacity = 0;
    return result;
  }

  for (int i = 0; i < unique; i++) {
    PollEvent* e = &((PollEvent*)result.data)[i];
    e->fd = fds[i];
    e->readable = (flags[i] & POLL_READ) != 0;
    e->writable = (flags[i] & POLL_WRITE) != 0;
    e->error = (flags[i] & 4) != 0;
  }

  if (fds) CARP_FREE(fds);
  if (flags) CARP_FREE(flags);
  return result;
}

void Poll_close(Poll p) {
  if (p.kq >= 0) close(p.kq);
}

#endif /* CARP_USE_KQUEUE */

#ifdef CARP_USE_EPOLL

typedef struct {
  int epfd;
} Poll;

Poll Poll_create_() {
  Poll p;
  p.epfd = epoll_create1(0);
  return p;
}

int Poll_fd_(Poll* p) { return p->epfd; }

int Poll_add_(Poll* p, int fd, int interest) {
  struct epoll_event ev;
  ev.events = 0;
  if (interest & POLL_READ) ev.events |= EPOLLIN;
  if (interest & POLL_WRITE) ev.events |= EPOLLOUT;
  ev.data.fd = fd;
  return epoll_ctl(p->epfd, EPOLL_CTL_ADD, fd, &ev) < 0 ? -1 : 0;
}

int Poll_modify_(Poll* p, int fd, int interest) {
  struct epoll_event ev;
  ev.events = 0;
  if (interest & POLL_READ) ev.events |= EPOLLIN;
  if (interest & POLL_WRITE) ev.events |= EPOLLOUT;
  ev.data.fd = fd;
  return epoll_ctl(p->epfd, EPOLL_CTL_MOD, fd, &ev) < 0 ? -1 : 0;
}

int Poll_remove_(Poll* p, int fd) {
  return epoll_ctl(p->epfd, EPOLL_CTL_DEL, fd, NULL) < 0 ? -1 : 0;
}

Array Poll_wait_(Poll* p, int timeout_ms) {
  struct epoll_event events[POLL_MAX_EVENTS];
  int n = epoll_wait(p->epfd, events, POLL_MAX_EVENTS, timeout_ms);

  Array result;
  if (n < 0) {
    result.len = 0; result.capacity = 0; result.data = NULL;
    return result;
  }

  result.len = n;
  result.capacity = n > 0 ? n : 1;
  result.data = CARP_MALLOC(result.capacity * sizeof(PollEvent));
  if (!result.data) {
    result.len = 0; result.capacity = 0;
    return result;
  }

  for (int i = 0; i < n; i++) {
    PollEvent* e = &((PollEvent*)result.data)[i];
    e->fd = events[i].data.fd;
    e->readable = (events[i].events & EPOLLIN) != 0;
    e->writable = (events[i].events & EPOLLOUT) != 0;
    e->error = (events[i].events & (EPOLLERR | EPOLLHUP)) != 0;
  }
  return result;
}

void Poll_close(Poll p) {
  if (p.epfd >= 0) close(p.epfd);
}

#endif /* CARP_USE_EPOLL */

#ifdef CARP_USE_POLL

typedef struct {
  struct pollfd *fds;
  int count;
  int capacity;
} Poll;

Poll Poll_create_() {
  Poll p;
  p.capacity = 16;
  p.count = 0;
  p.fds = (struct pollfd*)CARP_MALLOC(p.capacity * sizeof(struct pollfd));
  if (!p.fds) {
    p.capacity = 0;
  }
  return p;
}

int Poll_fd_(Poll* p) {
  /* No kernel fd — return 0 if valid, -1 if not */
  return (p->fds != NULL) ? 0 : -1;
}

int Poll_add_(Poll* p, int fd, int interest) {
  if (p->count >= p->capacity) {
    int new_cap = p->capacity * 2;
    struct pollfd *new_fds = (struct pollfd*)CARP_MALLOC(new_cap * sizeof(struct pollfd));
    if (!new_fds) return -1;
    memcpy(new_fds, p->fds, (size_t)p->count * sizeof(struct pollfd));
    CARP_FREE(p->fds);
    p->fds = new_fds;
    p->capacity = new_cap;
  }
  struct pollfd *pfd = &p->fds[p->count];
  pfd->fd = fd;
  pfd->events = 0;
  if (interest & POLL_READ) pfd->events |= POLLIN;
  if (interest & POLL_WRITE) pfd->events |= POLLOUT;
  pfd->revents = 0;
  p->count++;
  return 0;
}

int Poll_modify_(Poll* p, int fd, int interest) {
  for (int i = 0; i < p->count; i++) {
    if (p->fds[i].fd == fd) {
      p->fds[i].events = 0;
      if (interest & POLL_READ) p->fds[i].events |= POLLIN;
      if (interest & POLL_WRITE) p->fds[i].events |= POLLOUT;
      return 0;
    }
  }
  errno = ENOENT;
  return -1;
}

int Poll_remove_(Poll* p, int fd) {
  for (int i = 0; i < p->count; i++) {
    if (p->fds[i].fd == fd) {
      p->fds[i] = p->fds[p->count - 1];
      p->count--;
      return 0;
    }
  }
  errno = ENOENT;
  return -1;
}

Array Poll_wait_(Poll* p, int timeout_ms) {
  Array result;
  if (!p->fds) {
    result.len = 0; result.capacity = 0; result.data = NULL;
    return result;
  }

  int n = poll(p->fds, (nfds_t)p->count, timeout_ms);
  if (n < 0) {
    result.len = 0; result.capacity = 0; result.data = NULL;
    return result;
  }

  int ready = 0;
  for (int i = 0; i < p->count; i++) {
    if (p->fds[i].revents != 0) ready++;
  }

  result.len = ready;
  result.capacity = ready > 0 ? ready : 1;
  result.data = CARP_MALLOC(result.capacity * sizeof(PollEvent));
  if (!result.data) {
    result.len = 0; result.capacity = 0;
    return result;
  }

  int j = 0;
  for (int i = 0; i < p->count && j < ready; i++) {
    if (p->fds[i].revents == 0) continue;
    PollEvent* e = &((PollEvent*)result.data)[j];
    e->fd = p->fds[i].fd;
    e->readable = (p->fds[i].revents & POLLIN) != 0;
    e->writable = (p->fds[i].revents & POLLOUT) != 0;
    e->error = (p->fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
    j++;
  }

  result.len = j;
  return result;
}

void Poll_close(Poll p) {
  if (p.fds) CARP_FREE(p.fds);
}

#endif /* CARP_USE_POLL */

bool Poll_wait_failed_(Array* a) {
  return a->data == NULL;
}

Poll Poll_copy(Poll* p) {
  Poll c;
#ifdef CARP_USE_KQUEUE
  c.kq = p->kq;
#elif defined(CARP_USE_EPOLL)
  c.epfd = p->epfd;
#elif defined(CARP_USE_POLL)
  c.count = p->count;
  c.capacity = p->capacity;
  if (p->fds && p->capacity > 0) {
    c.fds = (struct pollfd*)CARP_MALLOC((size_t)p->capacity * sizeof(struct pollfd));
    if (!c.fds) {
      c.count = 0;
      c.capacity = 0;
    } else {
      memcpy(c.fds, p->fds, (size_t)p->count * sizeof(struct pollfd));
    }
  } else {
    c.fds = NULL;
  }
#endif
  return c;
}

#endif /* CARP_POLL_H */
