#pragma once

#include"Buffer.hh"
#include"ErrorWords.hh"
#include<string.h>

namespace cmw{

int32_t const chunk_size=8192;

class SendBufferFile:public SendBufferHeap
{
public:
  fileType hndl;

  SendBufferFile (int size):SendBufferHeap(size){}

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

template <class R>
class ReceiveBufferFile:public ReceiveBuffer<R>
{
  int bytesAvailable=0;
  int bufsize;

public:
  file_type hndl;

  explicit ReceiveBufferFile (int sz): ReceiveBuffer<R>(sz),bufsize(sz){}

  bool GotPacket ()
  {
    try{
      if(bytesAvailable<sizeof this->packetLength){
        bytesAvailable+=Read(hndl,this->buf+bytesAvailable
                               ,sizeof(this->packetLength)-bytesAvailable);
        if (bytesAvailable<(int32_t)sizeof this->packetLength) return false;
        this->index=0;
        this->msgLength=sizeof this->msgLength;
        this->msgLength=this->packetLength=this->template Give<uint32_t>();
        if (this->packetLength>this->bufsize) {
          throw failure("ReceiveBufferFile::GotPacket -- incoming size too great");
        }
      }

      bytesAvailable+=Read(hndl,this->buf+bytesAvailable
                           ,this->packetLength-bytesAvailable);
      if(bytesAvailable<this->packetLength)return false;

      bytesAvailable=0;
      return true;
    } catch(...){
      bytesAvailable=0;
      throw;
    }
  }

  void Reset ()
  {
    bytesAvailable=0;
  }
};
}

