//
// Created by swagger on 2022/6/28.
//

#include "acceptor.h"
#include <sys/socket.h>
#include <cerrno>
#include <iostream>
#include "input_message.h"
#include "socket.h"

using std::cout;
using std::endl;

Acceptor::Acceptor()
: listen_fd_(-1)
, socket_map_()
, accept_socket_(nullptr)
{}

Acceptor::~Acceptor() {
  StopAcceptor();
}

int Acceptor::StartAcceptor(int listen_fd, int timeout) {
  if ( listen_fd < 0 ) {
    return -1;
  }
  // 给Acceptor分配的Socket，当然，Create也会随之创建出一个epoll
  Socket* socket = Socket::Create(listen_fd, OnNewConnection, this);
  listen_fd_ = listen_fd;
  accept_socket_ = socket;
  return 0;
}

void Acceptor::StopAcceptor() {
  if ( accept_socket_ ) {
    accept_socket_->SetFailed();
  }
}

void Acceptor::OnNewConnection(Socket *socket) {
  // 在brpc中，这里更调用了OnNewConnectionUntilEAGAIN
  while (true) {
    // 失败了
    if ( socket->Failed() ) {
      break;
    }
    struct sockaddr_storage in_addr {};
    socklen_t in_len = sizeof(in_addr);
    int in_fd = ::accept(socket->fd(), (sockaddr*)&in_addr, &in_len);
    if ( in_fd < 0 ) {
      if ( errno == EAGAIN ) {
        cout << "EAGAIN" << endl;
        return;
      }
      cout << "got error in onNewConnection::accept" << endl;
      continue;
    }
    Acceptor* self = (Acceptor*)socket->user();
    if ( self == nullptr ) {
      cout << "error, user data == nullptr ???, not Acceptor*" << endl;
      return;
    }
    // self 也就是socket->user()
    Socket* in_socket = Socket::Create(in_fd, InputMessage::OnMessages, self);
    cout << "have new connection, add to epoll, open at fd: " << in_fd << endl;
    // TODO: add to socket map
  }
}

