#include "SendBufferCompressed.hh"
#include "ErrorWords.hh"
#include "IO.hh"
#include <string.h>

cmw::SendBufferCompressed::SendBufferCompressed (int sz) :
  SendBufferHeap(sz),compSize(sz+(sz>>3)+400)
  ,compressedBuf(new char[compSize])
{}

cmw::SendBufferCompressed::~SendBufferCompressed ()
{
  delete [] compressedBuf;
}

void cmw::SendBufferCompressed::Compress ()
{
  if(index+(index>>3)+400>compSize-compIndex)
    throw failure("Not enough room in compressed buf");
  compIndex+=::qlz_compress(buf,compressedBuf+compIndex
                            ,index,compress.state_compress);
  Reset();
}

bool
cmw::SendBufferCompressed::FlushFlush (::sockaddr* toAddr,::socklen_t toLen)
{
  int const bytes=sockWrite(sock_,compressedBuf,compIndex,toAddr,toLen);
  if(bytes==compIndex){compIndex=0;return true;}

  compIndex-=bytes;
  ::memmove(compressedBuf,compressedBuf+bytes,compIndex);
  return false;
}

bool
cmw::SendBufferCompressed::Flush (::sockaddr* toAddr,::socklen_t toLen)
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
