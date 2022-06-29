#include <iostream>
#include <unistd.h>
#include <thread>
#include "net/server.h"

using namespace std;

int port = 8008;

void thread_test();

int main() {

//  Server s;
//  if ( s.Start(port) != 0 ) {
//    cout << "Start failed " << endl;
//  }
//  cout << "Start at port: " << port << endl;
//  sleep(100);
//  cout << "main done" << endl;
  thread_test();
}

void child_2() {
  sleep(3);
}
void child_1() {
  std::thread child_child_thread(child_2);
  // 子线程直接退出，而child_2则继续运行
  // 不detach，那么错误
  child_child_thread.detach();
}

void thread_test() {
  std::thread child_thread_1(child_1);
  // 主线程进入随眠
  sleep(4);
  child_thread_1.detach();
}
