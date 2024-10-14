#include "wrapping_integers.hh"
#include <iostream>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  Wrap32 ckpt_32 = wrap( checkpoint, zero_point );
  uint64_t diff = raw_value_ - ckpt_32.raw_value_;

  if ( diff < ( 1UL << 31 ) || checkpoint < ( 1UL << 32 ) - diff ) {
    return checkpoint + diff;
  } else
    return checkpoint - ( ( 1UL << 32 ) - diff );
  // else{
  //   if((1UL << 31 < diff) && ((1UL << 32) - diff) <= checkpoint) {
  //     return checkpoint + diff;
  //   }
  //   else return checkpoint + ((1UL << 32) - (ckpt_32.raw_value_ - raw_value_));
  // }
}
