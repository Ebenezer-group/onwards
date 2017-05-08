#pragma once
#include"SendBuffer.hh"

namespace cmw{

class SendBufferHeap : public SendBuffer
{
public:
  SendBufferHeap (int sz):SendBuffer(new unsigned char[sz],sz)
  {}

  ~SendBufferHeap () { delete [] SendBuffer::buf; }
};
}
