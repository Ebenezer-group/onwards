#pragma once

#include"ErrorWords.hh"
#include"IO.hh"
#include"platforms.hh"
#include"SendBuffer.hh"
#include<string.h>

namespace cmw {

int32_t const chunk_size = 8192;

class SendBufferFile : public SendBufferHeap
{
public:
  file_type hndl;

  SendBufferFile (int size) : SendBufferHeap(size) {}

  bool Flush ()
  {
    int const bytes=Write(hndl,buf,index);

    if(bytes==index){
      index=0;
      return true;
    }

    index-=bytes;
    ::memmove(buf,buf+bytes,index);
    return false;
  }

  void ReceiveFile (int32_t)
  {
    throw failure("SendBufferFile::ReceiveFile not implemented");
  }

  int getBufsize()
  {
    return bufsize;
  }

  using SendBuffer::Receive;
};
}

