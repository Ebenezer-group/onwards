#pragma once

#include "quicklz.h"

namespace cmw {

struct qlz_compress_wrapper
{
  ::qlz_state_compress* state_compress;

  qlz_compress_wrapper ():state_compress(new ::qlz_state_compress()) {}
  ~qlz_compress_wrapper () {delete state_compress;}
  void reset () {::memset(state_compress,0,sizeof(::qlz_state_compress));}
};

struct qlz_decompress_wrapper
{
  ::qlz_state_decompress* state_decompress;

  qlz_decompress_wrapper ():state_decompress(new ::qlz_state_decompress()) {}
  ~qlz_decompress_wrapper () {delete state_decompress;}
  void reset () {::memset(state_decompress,0,sizeof(::qlz_state_decompress));}
};
}
