#pragma once

#include <string>

#include "conn.h"

class ConnShm : public IConn {
 public:
  ConnShm(const std::string& name);
  ~ConnShm();
  bool read(void* buf, size_t size) override;
  bool write(void* buf, size_t size) override;

 private:
  std::string m_name;
  void* m_mem;
  const size_t m_mem_size = 4096;
};
