//
// Created by swagger on 2022/6/28.
//

#ifndef RAIDEN_EVENT_DISPATCHER_H
#define RAIDEN_EVENT_DISPATCHER_H

#include <thread>
#include <atomic>

class Socket;

class EventDispatcher {
public:

  EventDispatcher();
  ~EventDispatcher();

  int Start();

  void Stop();

  void Join();

  int AddConsumer(Socket*, int fd);

  int AddEpollOut(Socket* socket, int fd, bool pollin);

private:

  static void Thread_start_entry(void* arg);

  void Run();

private:

  int wakeup_fd_[2];
  int epfd_;

  std::thread* thread_id_;

  std::atomic<bool> stop_;

};

EventDispatcher& GetGlobalEventDispatcher(int fd);

#endif //RAIDEN_EVENT_DISPATCHER_H
