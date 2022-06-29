//
// Created by swagger on 2022/6/28.
//

#include "input_message.h"
#include "socket.h"

InputMessage::InputMessage() {}

InputMessage::~InputMessage() {}

void InputMessage::OnMessages(Socket *m) {
  m->doRead(1024);
}