#pragma once

#include <mqueue.h>

#include <string>

#include "conn.h"

class ConnMQ : public IConn {
 public:
  ConnMQ() = delete;
  ConnMQ(const std::string& name);
  ~ConnMQ();
  bool read(void* buf, size_t size) override;
  bool write(void* buf, size_t size) override;

 private:
  mqd_t m_descriptor;
  size_t m_msg_size = 64;
  std::string m_queue_name;
  unsigned int m_default_prio = 10;
};
