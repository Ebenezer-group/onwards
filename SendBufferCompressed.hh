#pragma once
#include"ErrorWords.hh"
#include"IO.hh"
#include"qlz_wrapper.hh"
#include"SendBufferHeap.hh"
#include<string.h>

namespace cmw{

class SendBufferCompressed : public SendBufferHeap
{
  int compSize;
  int compIndex=0;
  char* compressedBuf;
  qlz_compress_wrapper compress;

  bool FlushFlush (::sockaddr* toAddr,::socklen_t toLen)
  {
    int const bytes=sockWrite(sock_,compressedBuf,compIndex,toAddr,toLen);
    if(bytes==compIndex){compIndex=0;return true;}

    compIndex-=bytes;
    ::memmove(compressedBuf,compressedBuf+bytes,compIndex);
    return false;
  }

public:
  SendBufferCompressed (int sz) : SendBufferHeap(sz),compSize(sz+(sz>>3)+400)
                                  ,compressedBuf(new char[compSize]) {}

  ~SendBufferCompressed ()
  { delete [] compressedBuf; }

  void Compress ()
  {
    if(index+(index>>3)+400>compSize-compIndex)
      throw failure("Not enough room in compressed buf");
    compIndex+=::qlz_compress(buf,compressedBuf+compIndex
                              ,index,compress.state_compress);
    Reset();
  }

  bool Flush (::sockaddr* toAddr=nullptr,::socklen_t toLen=0)
  {
    bool rc=true;
    bool haveWritten=false;
    if(compIndex>0){
      rc=FlushFlush(toAddr,toLen);
      haveWritten=true;
    }

    if(index>0){
      Compress();
      if(!haveWritten)rc=FlushFlush(toAddr,toLen);
      else rc=false;
    }
    return rc;
  }

  inline void CompressedReset ()
  {compIndex=0;compress.reset();Reset();}
};
}

