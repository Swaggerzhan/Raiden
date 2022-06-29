//
// Created by swagger on 2022/6/28.
//
#include <sys/epoll.h>
#include "event_dispatcher.h"
#include "socket.h"
#include <iostream>

using std::cout;
using std::endl;

using std::thread;

EventDispatcher::EventDispatcher()
: epfd_(-1)
, wakeup_fd_()
, thread_id_(nullptr)
, stop_(true)
{}

EventDispatcher::~EventDispatcher() {
  Stop();
  Join();
  if ( epfd_ >= 0 ) {
    close(epfd_);
    epfd_ = -1;
  }
  if ( wakeup_fd_[0] > 0 ) {
    close(wakeup_fd_[0]);
    close(wakeup_fd_[1]);
  }
}

int EventDispatcher::Start() {
  epfd_ = epoll_create(1024 * 1024);

  if (pipe(wakeup_fd_) != 0 ) {
    return -1;
  }

  if ( epfd_ < 0 ) {
    cout << "epoll_create return: " << epfd_ << endl;
    return -1;
  }
  stop_.store(false, std::memory_order_seq_cst);
  thread_id_ = new thread(Thread_start_entry, this);
  return 0;
}

void EventDispatcher::Stop() {
  stop_.store(true, std::memory_order_release);
  if ( epfd_ >= 0 ) {
    epoll_event evt = { EPOLLOUT, { nullptr }} ;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, wakeup_fd_[1], &evt);
  }
}

void EventDispatcher::Join() {
  thread_id_->join();
}

int EventDispatcher::AddConsumer(Socket* socket, int fd) {
  if ( epfd_ < 0 ) {
    return -1;
  }
  epoll_event evt;
  evt.events = EPOLLIN | EPOLLET;
  evt.data.ptr = socket;
  return epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &evt);
}

int EventDispatcher::AddEpollOut(Socket *socket, int fd, bool pollin) {
  if ( epfd_ < 0 ) {
    return -1;
  }

  epoll_event evt;
  evt.events = EPOLLOUT | EPOLLET;
  if ( pollin ) {
    evt.events |= EPOLLIN;
    if ( epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &evt) < 0 ) {
      return -1;
    }
  }else {
    if ( epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &evt) < 0 ) {
      return -1;
    }
  }
  return 0;
}



void EventDispatcher::Thread_start_entry(void *arg) {
  EventDispatcher* self = static_cast<EventDispatcher*>(arg);
  self->Run();
}

// 跑在一个
void EventDispatcher::Run() {
  while ( !stop_.load(std::memory_order_acquire) ) {
    epoll_event e[32];
    const int n = epoll_wait(epfd_, e, 32, -1);

    // 如果醒来发现是关闭信号，那么直接return即可
    if ( stop_.load(std::memory_order_acquire) ) {
      return;
    }
    if ( n < 0 ) {
      if ( EINTR == errno ) {
        continue;
      }
      break;
    }
    for (int i=0; i<n; ++i) {
      if ( e[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP) ) {
        // 这里Socket*可能是Acceptor的Socket，之后被调用的回调，就是
        // OnNewConnection了，而在内部，StartInputEvent会启用一个
        // 线/协程来去运行OnNewConnection，正常情况ET模式下，不会说出现
        // 2个线/协程同时运行OnNewConnection
        Socket::StartInputEvent((Socket*)e[i].data.ptr, e[i].events);
      }
    }
    for (int i=0; i<n; ++i) {
      if ( e[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP) ) {
        // TODO: Socket
      }
    }
  }
}

static EventDispatcher* g_edisp = nullptr;
static pthread_once_t g_edisp_once = PTHREAD_ONCE_INIT;

void InitEventDispatcher() {
  g_edisp = new EventDispatcher;
  if ( g_edisp->Start() != 0 ) {
    cout << "EventDispatcher Start failed" << endl;
  }
}

// 通过fd来获取一个全局的epoll
EventDispatcher& GetGlobalEventDispatcher(int fd) {
  pthread_once(&g_edisp_once, InitEventDispatcher);
  // brpc中进行了判断，分辨了是否存在多个EventDispatcher
  return g_edisp[0];
}
