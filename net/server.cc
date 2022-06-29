//
// Created by swagger on 2022/6/28.
//
#include "server.h"
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>
#include <cstring>
#include "acceptor.h"

using std::cout;
using std::endl;

// brpc中是位于endpoint
static int tcp_listen(int port) {
  struct sockaddr_storage serv_addr {};
  struct sockaddr_in* in4 = (struct sockaddr_in*)&serv_addr;
  in4->sin_family = AF_INET;
  in4->sin_port = htons(port);
  in4->sin_addr.s_addr = inet_addr("0.0.0.0");
  socklen_t serv_addr_size = sizeof(serv_addr);
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if ( sockfd < 0 ) {
    return -1;
  }
  // set reuse
  const int on = 1;
  if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                  &on, sizeof(on)) != 0 ) {
    return -1;
  }
  if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT,
                  &on, sizeof(on)) != 0 ) {
    return -1;
  }
  if ( ::bind(sockfd, (struct sockaddr*)&serv_addr, serv_addr_size) != 0 ) {
    cout << "bind error at sockfd because: " << strerror(errno) << endl;
    return -1;
  }
  if ( ::listen(sockfd, 65535) != 0 ) {
    return -1;
  }
  return sockfd;
}

Server::Server()
: a_(nullptr)
, port_(-1)
{}

Server::~Server() {
  if ( a_ ) {
    a_->StopAcceptor();
    delete a_;
  }
}

int Server::Start(int port) {
  int listen_fd = tcp_listen(port);
  if ( listen_fd < 0 ) {
    cout << "Server Start failed at port: " << port << endl;
    return -1;
  }
  port_ = port;
  // brpc: BuildAcceptor
  a_ = new Acceptor;
  if ( a_->StartAcceptor(listen_fd, 0) != 0 ) {
    cout << "Acceptor Start Failed " << endl;
    return -1;
  }
  return 0;
}

