//
// Created by swagger on 2022/6/28.
//
#include "socket.h"
#include "event_dispatcher.h"
#include <iostream>
#include "util/io_buf.h"

using std::cout;
using std::endl;


struct Socket::WriteRequest {
  static WriteRequest* const UNCONNECTED;
  Socket* socket;
  IOBuf data;
  WriteRequest* next;
};
Socket::WriteRequest* const Socket::WriteRequest::UNCONNECTED =
        (Socket::WriteRequest*)-1;


Socket::Socket()
: failed_(false)
, fd_(-1)
, on_edge_triggered_events_(nullptr)
, args(nullptr)
, user_(nullptr)
, write_head_(nullptr)
{}

Socket::~Socket() {}

Socket* Socket::Create(int fd, callback c, void* user) {
  Socket* self = new Socket;
  self->fd_ = fd;
  self->on_edge_triggered_events_ = c;
  self->args = self;
  self->user_ = user;
  // 将其添加到epoll中
  GetGlobalEventDispatcher(fd).AddConsumer(self, fd);
  return self;
}

struct ProcessEventArgs {
  callback on_edge_triggered_events;
  Socket* args;
};

static void ProcessEvent(void* args) {
  auto p = (ProcessEventArgs*)args;
  p->on_edge_triggered_events(p->args);
  delete p;
}

void Socket::StartInputEvent(Socket *self, uint32_t event) {
  if ( nullptr == self ) {
    return;
  }
  if ( self->fd() < 0 ) {
    return;
  }
  if ( !self->on_edge_triggered_events_ ) {
    return;
  }
  // TODO: 在brpc中，会启动一个bthread，运行ProcessEvent，其中所做的，就是
  // 调用on_edge_triggered_events来运行回调
  // 在这里，我们用线程来去代替这个操作
  // self->on_edge_triggered_events_(self->args);
  int x;
  ProcessEventArgs* p = new ProcessEventArgs;
  p->on_edge_triggered_events = self->on_edge_triggered_events_;
  p->args = self->args;
  std::thread t(ProcessEvent, p);
  t.detach();
  // self->on_edge_triggered_events_(self->args);
}

ssize_t Socket::doRead(size_t size_hint) {
  char buf[1024];
  ssize_t total_len = ::read(fd_, buf, size_hint);
  buf[total_len] = '\0';
  cout << "total_len: " << total_len << endl;
  cout << "got: " << buf << endl;
  return total_len;
}

ssize_t Socket::doWrite(WriteRequest *req) {
  IOBuf* data_list[16]; // brpc 256
  size_t ndata = 0;
  for (WriteRequest* p = req; p != nullptr && ndata < 16; p = p->next) {
    data_list[ndata ++] = &p->data;
  }
  return IOBuf::cut_multiple_into_file_descriptor(fd(), data_list, ndata);
}

int Socket::Write(IOBuf *data) {
  if ( data->empty() ) {
    return -1;
  }
  if ( Failed() ) {
    return -1;
  }
  WriteRequest* req = new WriteRequest;
  req->data.swap(data);
  req->next = WriteRequest::UNCONNECTED;

  // brpc中这里调用了StartWrite
  WriteRequest* const prev_head =
          write_head_.exchange(req, std::memory_order_release);
  // 如果对换出来发现并不是nullptr，那么就证明已经有其他线程处于写入过程中了
  // 结果就是，我们将新加入的东西链上之前的那块prev_head，那么就可以直接返回
  // 了，等到原先那个线程完成了`prev_head`写入后，自然就会写当前加入的Wreq
  //      write_head_
  //          |               ========>> 开始时刻，有一个线程在写n
  //          n
  //
  //      write_head_
  //          |               ========>> 新的线程想要写入req，但之前的n还未写完
  //          req -> n
  //
  //      write_head_         期望的写入顺序       write_head_
  //          |               ========>>             |
  //         req1 -> req -> n                        n -> req -> req1
  if ( prev_head != nullptr ) {
    req->next = prev_head;
    return 0;
  }

  // 如果对换出来，发现是nullptr，那么当前的线程就要充当写入着的身份了
  // 如果没有建立连接，分别有未建立连接和建立连接中
  req->next = nullptr;
  ssize_t nw = 0;
  // TODO: connect

  // 如果连接正常，这里才会真正的开始写入
  // brpc: if (conn_) {
  //    conn_->CutMessageIntoFileDescriptor(fd(), data_arr, 1);
  nw = req->data.cut_info_file_descriptor(fd(), 1024 * 1024);
  if ( nw < 0 ) {
    if ( errno != EAGAIN ) { // error
      SetFailed();
    }
  }else {
    // TODO: AddOutputBytes
  }

  if ( IsWriteComplete(req, true, nullptr) ) {

    return 0;
  }
  // 继续写
  // req2 -> req1 -> req -> nullptr
  std::thread new_thread(KeepWrite, req);
  new_thread.detach();
  return 0;
}

bool Socket::IsWriteComplete(Socket::WriteRequest *old_head,
                             bool singular_node,
                             Socket::WriteRequest **new_tail) {
  WriteRequest* new_head = old_head;
  WriteRequest* desired = nullptr; // 期望的
  bool return_when_no_more = true;
  // 当data中还有数据未写完时，那么肯定是需要继续写的，或者说传入singular表示为false表示
  // 还有其他的WriteRequest节点存在，也是需要继续写的
  if ( !old_head->data.empty() || !singular_node ) {
    desired = old_head;
    return_when_no_more = false;
  }
  // 如果期间没有人进行写入，那么write_head必然是不会发生改变的，exchange也随之会成功返回
  // 那么write_head会被改成nullptr，或者就还是本身`改成本身是因为还有数据没写完`
  if ( write_head_.compare_exchange_strong( new_head, desired,
                                            std::memory_order_acquire)) {
    if ( new_tail ) {
      *new_tail = old_head;
    }
    return return_when_no_more;
  }
  // 如果失败了，那么就证明write_head改变，有其他线程想要写入东西了，使得链表发生了改变，那么
  // 就需要进行链表的调整，然后继续写
  if ( old_head == new_head ) {
    cout << "Error, because old_head == new_head" << endl;
    sleep(10);
  }
  WriteRequest* tail = nullptr;
  WriteRequest* p = new_head;
  // 这里需要做的是进行链表反转，假设有这样的链表：
  //      new_head              old_head
  //          |                    |
  //        req2 -> req1 -> req -> n -> nullptr
  // 当写入完成后，n的数据已经被写到fd中了，通过交换write_head，我们可以得到新new_head
  // 进行链表反转后，可以得到真实的`用户期望`的写入顺序
  //                       write_head_
  //                          |
  //                          v
  //          req -> req1 -> req2 -> nullptr
  //           ^
  //           |
  // old_head: n
  do {
    while ( p->next == WriteRequest::UNCONNECTED ) {
      // 这种情况甚少发生，一般发生在其他线程调用Write，exchange成功不过next还未链上的情况
      std::this_thread::yield();
    }
    WriteRequest* const saved_next = p->next;
    p->next = tail;
    // tail和p向前移动一步
    tail = p;
    p = saved_next;
  } while ( p != old_head );
  old_head->next = tail; // tail就是req，或者说是下一个应该写入的WriteRequest

  // 自然的，new_head经过反转之后，就会成为新的尾巴
  if ( new_tail ) {
    *new_tail = new_head;
  }
  return false;
}

void* Socket::KeepWrite(void *args) {
  WriteRequest* req = static_cast<WriteRequest*>(args);
  WriteRequest* cur_tail = nullptr;
  Socket* socket = req->socket;
  do {
    if ( req->next != nullptr && req->data.empty() )  {
      WriteRequest* const saved_req = req;
      req = req->next;
      // 去掉已经写完的WriteRequest
      delete saved_req;
    }
    // doWrite会写入全部的WriteRequest数据，从req开始，到nullptr，`除了req前面的WriteRequest`
    const ssize_t nw = socket->doWrite(req);
    if ( nw < 0 ) {
      if ( errno != EAGAIN ) {
        socket->SetFailed();
        break;
      }
    }else {
      // TODO: AddOutputBytes
    }
    // 去掉所有已经写完的WriteRequest
    while ( req->next != nullptr && req->data.empty() ) {
      WriteRequest* const saved_req = req;
      req = req->next;
      delete saved_req;
    }
    if (nullptr == cur_tail) {
      for ( cur_tail = req; cur_tail->next != nullptr; cur_tail = cur_tail->next);
    }
    // 和之前Write不同，承担Write写入的线程一般是属于第一次写入，故而是false和尾巴是nullptr，因为只有一
    // 个WriteRequest
    // 而KeepWrite时可能有更多的数据在其中，一般可能是这个样子的：
    //                          req cur_tail
    //                           |    |
    //                           v    v
    //    req2 -> req1 -> req -> n -> n2 -> nullptr
    // 所以在KeepWrite中，我们总是尽量先将n和n2先写入fd，然后才会通过IsWriteComplete来判断是否完成了写入
    // 当然，如果没有完成，它也会进行排序，然后进入到下一轮循环中
    if ( socket->IsWriteComplete(cur_tail, (req == cur_tail), &cur_tail)) {
      delete req;
      return nullptr;
    }
    // 转完之后，就可能成这个样子：
    // req                       cur_tail
    //  |                           |
    //  v                           v
    //  n -> n2 -> req -> req1 -> req2 -> nullptr
    //                              ^
    //                              |
    //                         write_head_
    // 此时，在KeepWrite中，n和n2被写完了，自然会被清理，之后就从req开始写入，直到结束，当然，在此期间，可能还会
    // 有其他线程尝试写入`比如来了req3、req4、req5`，那么链表可能就会成这样：
    //                 req(code中关于WriteReqeust具柄，和下面的req表示的是一个`名字`罢了)
    //                  |
    //                  v
    //  req -> req1 -> req2 -> nullptr
    //                  ^
    //                  |
    //                 req3 <- req4 <- req5
    //                                  ^
    //                                  |
    //                               write_head_
    // 当KeepWrite写入完req到req2后，就会通过IsWriteComplete重新将其变为这种布局：
    //  req(同上，是代码中的具柄名)
    //   |
    //   v
    //  req2 -> req3 -> req4 -> req5 -> nullptr
    //                           ^
    //                           |
    //                       write_head_

  } while ( true );
  // TODO: release
  return nullptr;
}