//
// Created by swagger on 2022/6/28.
//

#ifndef RAIDEN_ACCEPTOR_H
#define RAIDEN_ACCEPTOR_H

#include <map>

class Socket;

class Acceptor {
public:

  explicit Acceptor();
  ~Acceptor();

  int StartAcceptor(int listen_fd, int timeout);
  void StopAcceptor();

  static void OnNewConnection(Socket* socket);

private:

  int listen_fd_;
  Socket* accept_socket_;

  std::map<int, Socket*> socket_map_;

};

#endif //RAIDEN_ACCEPTOR_H
