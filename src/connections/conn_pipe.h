#pragma once

#include <sched.h>

#include <cstddef>

#include "conn.h"

class ConnPipe : public IConn {
 public:
  ConnPipe();
  ~ConnPipe();
  bool read(void* buf, size_t size) override;
  bool write(void* buf, size_t size) override;

 private:
  int m_desc[2];
};
