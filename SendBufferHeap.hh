#pragma once
#include"SendBuffer.hh"

namespace cmw{
class SendBufferHeap:public SendBuffer{
public:
  inline SendBufferHeap (int sz):SendBuffer(new unsigned char[sz],sz){}

  inline ~SendBufferHeap (){delete [] SendBuffer::buf;}
};
}
