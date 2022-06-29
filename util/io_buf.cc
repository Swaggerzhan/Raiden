//
// Created by swagger on 2022/6/29.
//

#include "io_buf.h"
#include <unistd.h>
#include <malloc.h>
#include <new>
#include <atomic>
#include <sys/uio.h> // iovec
#include <algorithm> // min
#include <cstring>

struct IOBuf::Block {

  std::atomic<int> nshared; // 引用数量，当shared == 1 时，释放
  char* data;
  uint32_t size; // 目前使用容量
  uint32_t cap; // Block 容量

  Block(char* data_in, uint32_t data_size)
  : data(data_in)
  , cap(data_size)
  {

  }

  // 增加Block的引用数量
  void inc_ref() {
    nshared.fetch_add(1, std::memory_order_relaxed);
  }

  void dec_ref() {
    // 最后一个
    if ( nshared.fetch_sub(1, std::memory_order_release) == 1 )  {
      std::atomic_thread_fence(std::memory_order_acquire);
      this->~Block();
      free(this);
    }
  }

  int ref_count() {
    return nshared.load(std::memory_order_relaxed);
  }

  bool full() const { return size >= cap; }
  size_t left_space() const { return cap - size; }

};


// 创建出一个Block
IOBuf::Block* create_block(const size_t block_size) {
  char* mem = (char*)malloc(block_size);
  if ( mem == nullptr ) {
    return nullptr;
  }
  // placement new
  return new (mem) IOBuf::Block(mem, block_size - sizeof(IOBuf::Block));
}


IOBuf::Block* create_block() {
  return create_block(1024 * 1024 * 8); // 8K
}

IOBuf::Block* get_block(IOBuf::BigView* self) {
  IOBuf::Block* cur_block = self->ref_at(self->cur_offset_of_blockref).block;
  if ( cur_block->full() ) {
    // alloc
    auto blockref = new IOBuf::BlockRef;
    blockref->block = create_block();
    blockref->offset = sizeof(IOBuf::Block);
    self->cur_offset_of_blockref ++;
    self->refs[self->cur_offset_of_blockref] = *blockref;
  }else {
    return cur_block;
  }
}

int IOBuf::push_back(char c) {
  // TODO: thread local storage
  return 0;
}

int IOBuf::append(const void *data, size_t count) {
  if ( !data ) {
    return -1;
  }
  if ( count == 1 ) {
    return push_back(*((char const*)data));
  }
  size_t total_nc = 0;
  while ( total_nc < count ) {
    IOBuf::Block* b = BigView::GetBlock(&bv_);
    if ( !b ) {
      return -1;
    }
    const size_t nc = std::min(count - total_nc, b->left_space());
    memcpy(b->data + b->size, (char*)data + total_nc, nc);
    b->size += nc; // update
    total_nc += nc;
  }
  return 0;
}

IOBuf::BlockRef& IOBuf::_ref_at(int i) {
  IOBuf::BlockRef x;
  return x;
}

void IOBuf::swap(IOBuf *other) {

}

ssize_t IOBuf::cut_info_file_descriptor(int fd, size_t size_hint) {
  if ( empty() ) {
    return 0;
  }
  const size_t nref = bv_.nref;
  struct iovec vec[nref];
  size_t nvec = 0;
  size_t cur_len = 0;

  do {
    IOBuf::BlockRef const &r = _ref_at(nvec);
    vec[nvec].iov_base = r.block->data + r.offset;
    vec[nvec].iov_len = r.length;
    ++ nvec;
    cur_len += r.length;
  } while ( nvec < nref && cur_len < size_hint );

  ssize_t nw = ::writev(fd, vec, nvec);
  if ( nw > 0 ) {
    // TODO: 清理掉写入的数据，pop_front
  }
  return nw;
}