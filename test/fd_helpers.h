/* Helpers for the descriptor-exhaustion test */
#ifndef FD_TEST_HELPERS_H
#define FD_TEST_HELPERS_H

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <unistd.h>

static int fd_test_held[FD_SETSIZE];
static int fd_test_count = 0;

void FdTest_release_() {
  while (fd_test_count > 0) close(fd_test_held[--fd_test_count]);
}

/* Holds every free fd below FD_SETSIZE, raising the soft RLIMIT_NOFILE to
   2 * FD_SETSIZE first if needed. Returns false, holding nothing, on failure. */
bool FdTest_occupy_MINUS_low_() {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) != 0) return false;
  if (rl.rlim_cur < 2 * FD_SETSIZE) {
    if (rl.rlim_max < 2 * FD_SETSIZE) return false;
    rl.rlim_cur = 2 * FD_SETSIZE;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0) return false;
  }
  int fd = open("/dev/null", O_RDONLY);
  while (fd >= 0) {
    fd_test_held[fd_test_count++] = fd;
    if (fd >= FD_SETSIZE - 1) return true;
    fd = dup(fd);
  }
  FdTest_release_();
  return false;
}

#endif
