#pragma once

#include <sched.h>

#include <cstddef>

enum ConnectionType { PIPE, MQ, SHM };

class IConn {
 public:
  virtual ~IConn(){};
  virtual bool write(void* buf, size_t size) = 0;
  virtual bool read(void* buf, size_t size) = 0;

 protected:
  pid_t m_host_pid;
};
