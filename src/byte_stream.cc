#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_(capacity),
  head(0),
  tail(0),
  cur_size(0),
  available_size(capacity),
  pushed_bytes_nums(0),
  poped_bytes_nums(0),
  is_closed_(false),
  error_occurred(false),
  buffer(capacity+1) {
}


void Writer::push( string data )
{
  if(is_closed_) {
    error_occurred = true;
    // throw logic_error("流已经关闭了！！！");
    return ;
  }

  if(data.size() > available_size) {
    error_occurred = true;
    // throw logic_error("太多了，不要在进来了！！！");
    for(size_t i=0; i<available_size; i++){
      buffer[tail] = data[i];
      tail = (tail + 1) % (capacity_+1);
    }
    cur_size += available_size;
    pushed_bytes_nums += available_size;
    available_size -= available_size;
  } else {
    for(uint64_t i=0; i<data.size(); i++){
      buffer[tail] = data[i];
      tail = (tail + 1) % (capacity_+1);
    }
    cur_size += data.size();
    available_size -= data.size();
    pushed_bytes_nums += data.size();
  }
  (void)data;
}

void Writer::close()
{
  is_closed_ = true;
}

void Writer::set_error()
{
  error_occurred = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return available_size;
}

uint64_t Writer::bytes_pushed() const
{
  return pushed_bytes_nums;
}

string_view Reader::peek() const
{
  return string_view(&buffer[head], 1);
}

bool Reader::is_finished() const
{
  if(head == tail && is_closed_)
    return true;
  else return false;
}

bool Reader::has_error() const
{
  return error_occurred;
}

void Reader::pop( uint64_t len )
{
  if(len >cur_size){
    error_occurred = true;
    // throw logic_error("给不了这么多！！！");
    return ;
  } else {
    head = (head+len) % (capacity_+1);
    cur_size -= len;
    available_size += len;
    poped_bytes_nums += len;
  }
  (void)len;
}

uint64_t Reader::bytes_buffered() const
{
  return cur_size;
}

uint64_t Reader::bytes_popped() const
{
  return poped_bytes_nums;
}
