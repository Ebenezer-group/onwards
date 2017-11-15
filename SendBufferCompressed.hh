#pragma once
#include"ErrorWords.hh"
#include"IO.hh"
#include"quicklz.h"
#include"SendBuffer.hh"
#include<string.h>

namespace cmw{
struct compress_wrapper{
  ::qlz_state_compress* qlz_compress;
  compress_wrapper ():qlz_compress(new ::qlz_state_compress()){}
  ~compress_wrapper (){delete qlz_compress;}
};

class SendBufferCompressed:public SendBufferHeap{
  compress_wrapper compress;
  int compSize;
  int compIndex=0;
  char* compressedBuf;

  bool FlushFlush (::sockaddr* toAddr,::socklen_t toLen){
    int const bytes=sockWrite(sock_,compressedBuf,compIndex,toAddr,toLen);
    if(bytes==compIndex){compIndex=0;return true;}

    compIndex-=bytes;
    ::memmove(compressedBuf,compressedBuf+bytes,compIndex);
    return false;
  }

public:
  SendBufferCompressed (int sz):SendBufferHeap(sz),compSize(sz+(sz>>3)+400)
                                ,compressedBuf(new char[compSize]){}

  ~SendBufferCompressed (){delete[] compressedBuf;}

  bool Flush (::sockaddr* toAddr=nullptr,::socklen_t toLen=0){
    bool rc=true;
    if(compIndex>0)rc=FlushFlush(toAddr,toLen);

    if(index>0){
      if(index+(index>>3)+400>compSize-compIndex)
        throw failure("Not enough room in compressed buf");
      compIndex+=::qlz_compress(buf,compressedBuf+compIndex
                                ,index,compress.qlz_compress);
      Reset();
      if(rc)rc=FlushFlush(toAddr,toLen);
    }
    return rc;
  }

  inline void CompressedReset (){Reset();compIndex=0;
    ::memset(compress.qlz_compress,0,sizeof(::qlz_state_compress));
  }
};
}
