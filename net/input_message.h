//
// Created by swagger on 2022/6/28.
//

#ifndef RAIDEN_INPUT_MESSAGE_H
#define RAIDEN_INPUT_MESSAGE_H

#endif //RAIDEN_INPUT_MESSAGE_H

class Socket;

class InputMessage {
public:

  explicit InputMessage();
  ~InputMessage();

  static void OnMessages(Socket* m);

private:

};
