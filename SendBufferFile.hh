#pragma once

#include "platforms.hh"
#include "SendBufferHeap.hh"

namespace cmw {

int32_t const chunk_size = 8192;

class SendBufferFile : public SendBufferHeap
{
public:
  file_type hndl;

  SendBufferFile (int);
  bool Flush ();
  void ReceiveFile (int32_t fl_sz);

  int getBufsize()
  {
    return bufsize;
  }

  using SendBuffer::Receive;
};
}

