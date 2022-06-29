//
// Created by swagger on 2022/6/29.
//

#ifndef RAIDEN_IO_BUF_H
#define RAIDEN_IO_BUF_H

#include <atomic>
#include <unistd.h>

class IOBuf {
public:

  struct Block;

  struct BlockRef {
    uint32_t offset; // Block中真正数据的起始位置，mem = Block + data
    uint32_t length; // data的长度
    Block* block;
  };

  // brpc中，还有一个由2个BlockRef构成的SmallView

  struct BigView {
    uint32_t start; // 先设置为0
    BlockRef* refs; // BlockRef数组
    uint32_t nref; // BlockRef数组的大小
    size_t bytes;
    uint32_t cap_mask; // cap = cap_mask + 1

    uint32_t cur_offset_of_blockref;

    BlockRef& ref_at(uint32_t i)
    { return refs[start + i]; }

    uint32_t capacity()
    { return cap_mask + 1; }

  };

  IOBuf::BlockRef& _ref_at(int i);

  void swap(IOBuf* other);

  int push_back(char c);

  // 将data的数据复制到IOBuf中，涉及到复制
  // -1 为失败
  int append(void const* data, size_t count);

  inline bool empty() { return false; }


  // 将数据刷入到fd中
  ssize_t cut_info_file_descriptor(int fd, size_t size_hint);

  static ssize_t cut_multiple_into_file_descriptor(int fd, IOBuf* const* pieces,
                                                   size_t count);


private:
  BigView bv_;
};



#endif //RAIDEN_IO_BUF_H
