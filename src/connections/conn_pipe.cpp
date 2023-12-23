#include "conn_pipe.h"

#include <poll.h>
#include <sys/syslog.h>
#include <unistd.h>

#include <string>

#include "conn.h"

ConnPipe::ConnPipe() {
  int status = pipe(m_desc);
  if (status == -1) {
    syslog(LOG_ERR, "ERROR: creating connection, in pipe()");
    exit(1);
  }

  syslog(LOG_INFO, "INFO: Pipe created");
}

ConnPipe::~ConnPipe() {
  close(m_desc[0]);
  close(m_desc[1]);
}

bool ConnPipe::write(void* buf, size_t size) {
  bool res;
  res = ::write(m_desc[1], buf, size);
  return res;
}

bool ConnPipe::read(void* buf, size_t size) {
  int res = -1;
  int fd = m_desc[0];
  struct pollfd tmp;
  tmp.fd = fd;
  tmp.events = POLLIN;

  if (poll(&tmp, 1, 0) == 1) {
    res = ::read(fd, buf, size);
  }

  return res > 0;
}
