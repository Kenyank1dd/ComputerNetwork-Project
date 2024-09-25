#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : 
  capacity_( capacity ),
  error_(false),
  is_close(false),
  num_bytes_pushed(0),
  num_bytes_popped(0),
  num_bytes_buffered(0),
  bytes_queue() {}

bool Writer::is_closed() const
{
  return is_close;
}

void Writer::push( string data )
{
  if (is_close || available_capacity() == 0 || data.size() == 0) return;

  if (available_capacity() < data.size()) {
    data = data.substr(0, available_capacity());
  }
  num_bytes_buffered += data.size();
  num_bytes_pushed += data.size();
  bytes_queue.push_back(std::move(data));

  return;
}

void Writer::close()
{
  is_close = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - num_bytes_buffered;
}

uint64_t Writer::bytes_pushed() const
{
  return num_bytes_pushed;
}

bool Reader::is_finished() const
{
  return is_close && num_bytes_buffered == 0;
}

uint64_t Reader::bytes_popped() const
{
  return num_bytes_popped;
}

string_view Reader::peek() const
{
  string_view sw(bytes_queue.front());
  return sw;
}

void Reader::pop( uint64_t len )
{
  uint64_t n = min(len, num_bytes_buffered);
  while(n > 0) {
    if(n < bytes_queue.front().size()) {
      bytes_queue.front().erase(0, n);
      num_bytes_buffered -= n;
      num_bytes_popped += n;
      n = 0;
    }
    else {
      n -= bytes_queue.front().size();
      num_bytes_buffered -= bytes_queue.front().size();
      num_bytes_popped += bytes_queue.front().size();
      bytes_queue.pop_front();
    }
  }
}

uint64_t Reader::bytes_buffered() const
{
  return num_bytes_buffered;
}
