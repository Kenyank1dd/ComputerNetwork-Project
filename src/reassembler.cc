#include "reassembler.hh"
#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  uint64_t capacity = output_.writer().available_capacity();
  if ( output_.writer().is_closed() || capacity == 0 || first_index >= expect_idx + capacity )
    return;
  if ( first_index + data.length() <= expect_idx && !is_last_substring )
    return;

  if ( is_last_substring )
    last_idx = first_index + data.length();
  if ( first_index + data.length() >= expect_idx + capacity ) {
    data.resize( expect_idx + capacity - first_index );
    is_last_substring = false;
  }
  uint64_t idx = 0, pos = cache.size();
  bool found_pos = false;
  for ( auto iter = cache.begin(); iter != cache.end(); ) {
    auto& [cur_first_idx, cur_data] = *iter;
    if ( first_index > cur_first_idx + cur_data.length() ) { // totally after
      idx++;
      iter++;
      continue;
    }
    if ( first_index + data.length() < cur_first_idx ) { // totally before
      if ( !found_pos )
        pos = idx;
      break;
    } else {
      if ( !found_pos ) {
        pos = idx;
        found_pos = true;
      }
      if ( first_index < cur_first_idx ) {
        if ( first_index + data.length() <= cur_first_idx + cur_data.length() ) {
          data.resize( cur_first_idx - first_index );
          data += cur_data;
        }
        num_bytes_pending -= cur_data.length();
      } else {
        if ( first_index + data.length() >= cur_first_idx + cur_data.length() ) {
          num_bytes_pending -= cur_data.length();
          cur_data.resize( first_index - cur_first_idx );
          data = cur_data + data;
        } else {
          num_bytes_pending -= cur_data.length();
          data = cur_data;
        }
        first_index = cur_first_idx;
      }
      iter = cache.erase( iter );
    }
    idx++;
  }
  auto iter = cache.begin();
  advance( iter, pos );
  num_bytes_pending += data.length();
  cache.insert( iter, make_pair( first_index, std::move( data ) ) );

  if ( expect_idx >= cache.front().first ) {
    cache.front().second.erase( 0, expect_idx - cache.front().first );
    num_bytes_pending -= expect_idx - cache.front().first;
    expect_idx += cache.front().second.length();
    num_bytes_pending -= cache.front().second.length();
    output_.writer().push( std::move( cache.front().second ) );
    cache.pop_front();
  }

  if ( expect_idx == last_idx && cache.empty() ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return num_bytes_pending;
}
