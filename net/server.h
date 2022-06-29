//
// Created by swagger on 2022/6/28.
//

#ifndef RAIDEN_SERVER_H
#define RAIDEN_SERVER_H

class Acceptor;

class Server {
public:

  explicit Server();
  ~Server();

  int Start(int port);

private:

  int port_;
  Acceptor* a_;

};

#endif //RAIDEN_SERVER_H
