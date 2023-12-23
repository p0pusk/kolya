#include "conn_mq.h"

#include <fcntl.h>
#include <sys/syslog.h>
#include <unistd.h>

#include <cstddef>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

ConnMQ::ConnMQ(const std::string &name) {
  m_queue_name = name + std::to_string(getpid());
  m_host_pid = getpid();

  mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 1;
  attr.mq_msgsize = m_msg_size;
  attr.mq_curmsgs = 0;

  m_descriptor =
      mq_open(m_queue_name.c_str(), O_RDWR | O_CREAT | O_NONBLOCK, 0666, &attr);

  if (m_descriptor == -1) {
    syslog(LOG_ERR, "ERROR: Couldn't open shared memory");
    throw std::runtime_error("Couldn't open shared queue");
  }
}

ConnMQ::~ConnMQ() {
  if (mq_close(m_descriptor) == -1) {
    syslog(LOG_ERR, "ERROR: Couldn't close shm descriptor");
    std::exit(1);
  }

  if (getpid() == m_host_pid) {
    if (mq_unlink(m_queue_name.c_str()) == -1) {
      syslog(LOG_ERR, "ERROR: Couldn't unlink shared memory");
      std::exit(1);
    }
  }
}

bool ConnMQ::read(void *buf, size_t size) {
  return mq_receive(m_descriptor, (char *)buf, size, nullptr) > 0;
}

bool ConnMQ::write(void *buf, size_t size) {
  return mq_send(m_descriptor, (const char *)buf, size, 0) != -1;
}
