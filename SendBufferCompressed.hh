#pragma once

#include "qlz_wrapper.hh"
#include "SendBufferHeap.hh"

namespace cmw {

class SendBufferCompressed : public SendBufferHeap
{
  int compSize;
  int compIndex=0;
  char* compressedBuf;
  qlz_compress_wrapper compress;

  bool FlushFlush (::sockaddr*,::socklen_t);

public:
  SendBufferCompressed (int sz);
  ~SendBufferCompressed ();
  void Compress ();
  bool Flush (::sockaddr* =nullptr,::socklen_t=0);

  inline void CompressedReset ()
  {compIndex=0;compress.reset();Reset();}
};
}
