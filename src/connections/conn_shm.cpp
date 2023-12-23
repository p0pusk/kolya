#include "conn_shm.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syslog.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <ostream>
#include <stdexcept>

ConnShm::ConnShm(const std::string &name) {
  m_name = name;
  m_host_pid = getpid();
  int fd = shm_open(m_name.c_str(), O_RDWR | O_CREAT, 0666);
  if (fd == -1)
    throw std::runtime_error("Couldn't open shared memory file descriptor");

  ftruncate(fd, m_mem_size);
  m_mem = mmap(NULL, m_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (m_mem == MAP_FAILED) {
    throw std::runtime_error("Couldn't allocate shared memory with mmap");
  }
}

ConnShm::~ConnShm() {
  if (munmap(m_mem, m_mem_size) == -1) {
    syslog(LOG_ERR, "ERROR: Couldn't free shared memory");
    std::exit(1);
  }

  if (getpid() == m_host_pid) {
    if (shm_unlink(m_name.c_str()) == -1) {
      syslog(LOG_ERR, "ERROR: Couldn't unlink shared memory");
      std::exit(1);
    }
  }
}

bool ConnShm::read(void *buf, size_t size) {
  std::memcpy(buf, m_mem, size);
  std::memset(m_mem, 0, m_mem_size);
  return std::strlen((char *)buf) != 0;
}

bool ConnShm::write(void *buf, size_t size) {
  if (size <= 0) return false;
  std::memcpy(m_mem, buf, size);
  return true;
}
