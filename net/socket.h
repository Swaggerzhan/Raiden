//
// Created by swagger on 2022/6/28.
//

#ifndef RAIDEN_SOCKET_H
#define RAIDEN_SOCKET_H

#include <unistd.h>
#include <cstdint>
#include <atomic>

class Socket;
class IOBuf;
typedef void (*callback)(Socket*);

// Socket的User数据，在brpc中，是InputMessage
// 在Acceptor的Start中，传入的user数据是 Acceptor*
// 而在OnNewConnection中，传入的也是Acceptor*
class Socket {
  struct WriteRequest;
public:

  explicit Socket();
  ~Socket();

  inline const int fd() { return fd_; }

  inline void* user() { return user_; }
  inline void set_user(void* m) { user_ = m; }

  static void StartInputEvent(Socket* socket, uint32_t event);

  static Socket* Create(int fd, callback c, void* user);

  // 用来去模拟brpc中的SetFailed
  inline void SetFailed() { failed_.store(true); }
  inline bool Failed() { return failed_.load(); }

  //////
  ssize_t doRead(size_t size_hint);
  ssize_t doWrite(WriteRequest* req);

  int Write(IOBuf* data);
  bool IsWriteComplete(Socket::WriteRequest* old_head,
                       bool singular_node,
                       Socket::WriteRequest** new_tail);
  static void* KeepWrite(void* args);

private:
  std::atomic<bool> failed_;

  int fd_;

  // 回调
  callback on_edge_triggered_events_;
  Socket* args;

  void* user_; // 用户数据

  std::atomic<WriteRequest*> write_head_;

};

#endif //RAIDEN_SOCKET_H
