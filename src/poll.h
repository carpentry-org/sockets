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

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
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

int Poll_modify_(Poll* p, int fd, int interest) {
  /* Remove both, then add desired */
  struct kevent changes[4];
  int n = 0;
  EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  kevent(p->kq, changes, n, NULL, 0, NULL); /* ignore errors from deleting non-existent */
  return Poll_add_(p, fd, interest);
}

int Poll_remove_(Poll* p, int fd) {
  struct kevent changes[2];
  EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
  EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
  kevent(p->kq, changes, 2, NULL, 0, NULL); /* ignore errors */
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
    flags = (int*)CARP_MALLOC(n * sizeof(int));

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

Poll Poll_copy(Poll* p) {
  Poll c;
#ifdef CARP_USE_KQUEUE
  c.kq = p->kq;
#elif defined(CARP_USE_EPOLL)
  c.epfd = p->epfd;
#endif
  return c;
}

#endif /* CARP_POLL_H */
