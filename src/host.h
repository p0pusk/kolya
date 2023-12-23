#pragma once

#include <sched.h>
#include <semaphore.h>

#include <string>
#include <unordered_map>

#include "client.h"
#include "conn.h"

class Host {
 public:
  static Host& getInstance();
  void create_client(ConnectionType id);
  void run();

  Host(Host const&) = delete;
  void operator=(Host const&) = delete;

  inline static std::unordered_map<pid_t, Client*> s_clients;

 private:
  Host();
  ~Host();

  void fork_terminal();
  void open_terminal();

  const std::filesystem::path m_term_in = "/tmp/lab2/host.in";
  const std::filesystem::path m_term_out = "/tmp/lab2/host.out";
  sem_t* m_sem_client;
  sem_t* m_sem_host;
  sem_t* m_sem_write;
  inline static pid_t s_pid_host;
  inline static pid_t s_pid_terminal;
};
