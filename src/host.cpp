#include "host.h"

#include <fcntl.h>
#include <linux/prctl.h>
#include <semaphore.h>
#include <sys/prctl.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "client.h"
#include "conn.h"
#include "conn_pipe.h"

Host& Host::getInstance() {
  static Host instance;
  return instance;
}

Host::Host() {
  s_pid_host = getpid();
  m_sem_client = sem_open("/client", 0);
  if (m_sem_client == SEM_FAILED) {
    sem_close(m_sem_client);
    syslog(LOG_ERR, "ERROR: failed to open semaphore");
    exit(1);
  }

  m_sem_host = sem_open("/host", 0);
  if (m_sem_host == SEM_FAILED) {
    sem_close(m_sem_host);
    syslog(LOG_ERR, "ERROR: failed to open semaphore");
    exit(1);
  }

  m_sem_write = sem_open("/sem_write", 1);
  if (m_sem_write == SEM_FAILED) {
    sem_close(m_sem_write);
    syslog(LOG_ERR, "ERROR: failed to open semaphore");
    exit(1);
  }

  std::filesystem::remove_all("/tmp/lab2");
  if (!std::filesystem::exists("/tmp/lab")) {
    std::filesystem::create_directory("/tmp/lab2");
  }
  std::ofstream in(m_term_in);
  std::ofstream out(m_term_out);

  std::signal(SIGTERM, [](int signum) {
    if (getpid() != s_pid_host) exit(0);
    for (auto& c : s_clients) {
      syslog(LOG_DEBUG, "%s",
             ("DEBUG: killing " + std::to_string(c.first)).c_str());
      kill(c.first, SIGTERM);
    }

    syslog(LOG_INFO, "INFO: Host terminating");
    exit(0);
  });

  signal(SIGCHLD, [](int) {
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
      if (pid == s_pid_terminal) {
        syslog(LOG_INFO, "INFO: host terminal closed, host terminating");
        kill(getpid(), SIGTERM);
      }

      syslog(LOG_DEBUG, "DEBUG: caught terminating child %d", pid);
      delete s_clients[pid];
      s_clients.erase(pid);

      if (s_clients.size() == 0) {
        syslog(LOG_INFO, "INFO: No remaining clients, host terminating");
        exit(0);
      }
    }
  });
}

Host::~Host() {
  sem_close(m_sem_client);
  sem_close(m_sem_host);

  for (auto& client : s_clients) {
    delete client.second;
  }
}

void Host::create_client(ConnectionType id) {
  syslog(LOG_INFO, "INFO: Creating connection...");
  Client* client = new Client(id);

  client->run();

  if (getpid() == s_pid_host) {
    s_clients[client->m_pid_client] = client;
  }
}

void Host::run() {
  fork_terminal();
  std::fstream term(m_term_out);
  term << "You are in host terminal" << std::endl;

  const int buf_size = 1000;
  char buf[buf_size];
  while (true) {
    sem_wait(m_sem_client);
    syslog(LOG_DEBUG, "DEBUG: host aquired client-read semaphore");
    for (auto& c : s_clients) {
      syslog(LOG_DEBUG, "DEBUG: host reading %d", c.first);
      if (c.second->m_conn->read(buf, buf_size)) {
        syslog(LOG_DEBUG, "DEBUG: host read \"%s\" from %d", buf, c.first);
        term << "[" << c.first << "]: " << buf << std::endl;
      } else {
        syslog(LOG_DEBUG, "DEBUG: host read nothing from %d", c.first);
      }
    }
  }
}

void Host::open_terminal() {
  prctl(PR_SET_PDEATHSIG, SIGTERM);
  int res = execl("/usr/bin/kitty", "kitty", "--", "bash", "-c",
                  ("cp /dev/stdin " + m_term_in.string() + " | tail -f " +
                   m_term_out.string())
                      .c_str(),
                  (char*)NULL);
  if (res < 0) {
    syslog(LOG_ERR, "ERROR: in execl(), client terminating");
    exit(1);
  }
}

void Host::fork_terminal() {
  switch (pid_t pid = fork()) {
    case -1:
      syslog(LOG_ERR, "ERROR: in fork(), host terminating");
      exit(1);
      break;
    case 0:
      s_pid_terminal = pid;
      break;
    default:
      s_pid_terminal = pid;
      return;
  }

  prctl(PR_SET_NAME, "host_term_listener");
  prctl(PR_SET_PDEATHSIG, SIGTERM);

  switch (fork()) {
    case -1:
      syslog(LOG_ERR, "ERROR: in fork(), host terminating");
      exit(1);
      break;
    case 0:
      open_terminal();
      break;
  }

  std::signal(SIGCHLD, [](int) {
    syslog(LOG_INFO, "INFO: host terminal closed => host listener terminating");
    kill(getpid(), SIGKILL);
  });

  std::ifstream tin(m_term_in);
  const int buf_size = 1000;
  char buf[buf_size];
  std::string line;
  while (true) {
    if (std::getline(tin, line) && line.size() != 0) {
      sem_wait(m_sem_write);
      syslog(LOG_DEBUG, "DEBUG: host aquired write semaphore");
      strcpy(buf, line.c_str());
      for (auto& c : s_clients) {
        c.second->m_conn->write(
            buf, std::min(sizeof(line), sizeof(char) * buf_size));
        syslog(LOG_DEBUG, "DEBUG: host wrote \"%s\" to client[%d]", buf,
               c.first);
      }
      for (int i = 0; i < s_clients.size(); i++) {
        sem_post(m_sem_host);
      }
      sem_post(m_sem_write);
      syslog(LOG_DEBUG, "DEBUG: host released write semaphore");
    }
    tin.clear();
  }
}

void open_semaphores(sem_t* sem_host, sem_t* sem_client, sem_t* sem_write) {
  sem_unlink("/host");
  sem_host =
      sem_open("/host", O_CREAT | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO, 0);
  errno = 0;
  if (sem_host == SEM_FAILED) {
    sem_close(sem_host);
    std::cerr << "sem_open() failed: " << errno << std::endl;
    exit(1);
  }

  sem_unlink("/client");
  sem_client =
      sem_open("/client", O_CREAT | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO, 0);
  errno = 0;
  if (sem_client == SEM_FAILED) {
    sem_close(sem_client);
    sem_close(sem_host);
    std::cerr << "sem_open() failed: " << errno << std::endl;
    exit(1);
  }

  sem_unlink("/sem_write");
  sem_write =
      sem_open("/sem_write", O_CREAT | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO, 1);
  errno = 0;
  if (sem_write == SEM_FAILED) {
    sem_close(sem_client);
    sem_close(sem_host);
    sem_close(sem_write);
    std::cerr << "sem_open() failed: " << errno << std::endl;
    exit(1);
  }
}

auto main() -> int {
  openlog("host", LOG_NDELAY | LOG_PID, LOG_DAEMON);
  sem_t *sem_host, *sem_client, *sem_write;
  open_semaphores(sem_host, sem_client, sem_write);

  syslog(LOG_INFO, "INFO: host starting...");
  Host& host = Host::getInstance();
  host.create_client(ConnectionType::PIPE);
  host.create_client(ConnectionType::MQ);
  host.create_client(ConnectionType::SHM);

  host.run();
  sem_destroy(sem_host);
  sem_destroy(sem_client);
  syslog(LOG_INFO, "INFO: Timeout");
  closelog();
}
