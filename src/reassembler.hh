#pragma once

#include "byte_stream.hh"

#include <string>
#include <vector>
#include <tuple>

using namespace std;

class Reassembler
{
  vector<vector<string>> buffer; // 未按序到达的缓存起来
  uint64_t cur_need_index;  // 当前想要的数据起始字节
  uint64_t bytes_in_buffer; // 在缓存中的字节数
public:
  explicit Reassembler( );
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;
  // 存进缓冲区
  void push_in_buffer(uint64_t first_index, string data, bool is_last_substring);
  // 提交给流
  void put_in_stream(Writer& output);
  void last_empty_flag(uint64_t last_pos);// 处理有结束标志的空串
  void output_buffer();//输出buffer调试用
  void insert_SYN () {cur_need_index++;}; // 插入SYN
  void insert_FIN () {cur_need_index++;}; // 插入FIN
};
