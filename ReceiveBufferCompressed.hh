#pragma once

#include"ErrorWords.hh"
#include"IO.hh"
#include"platforms.hh"
#include"qlz_wrapper.hh"
#include"ReceiveBuffer.hh"
#include<string.h>

namespace cmw {
template <class R>
class ReceiveBufferCompressed : public ReceiveBuffer<R>
{
  int const bufsize;
  int bytesRead=0;
  int compressedSize;
  char* compressedStart;
  qlz_decompress_wrapper decompress;

public:
  sock_type sock_;

  explicit ReceiveBufferCompressed (int size):ReceiveBuffer<R>(new char[size],0),bufsize(size)
  {}

  ~ReceiveBufferCompressed () {delete [] this->buf;}

  bool GotPacket ()
  {
    try{
      if(bytesRead<9) {
        bytesRead+=sockRead(sock_,this->buf+bytesRead,9-bytesRead);
        if(bytesRead<9)return false;

        if((this->packetLength=::qlz_size_decompressed(this->buf))>bufsize)
          throw failure("Decompress::GotPacket decompressed size too big ")
                         <<this->packetLength<<" "<<bufsize;

        if((compressedSize=::qlz_size_compressed(this->buf))>bufsize)
          throw failure("Decompress::GotPacket compressed size too big");

        compressedStart=this->buf+bufsize-compressedSize;
        ::memmove(compressedStart,this->buf,9);
      }
      bytesRead+=sockRead(sock_,compressedStart+bytesRead
                            ,compressedSize-bytesRead);
      if(bytesRead<compressedSize)return false;

      ::qlz_decompress(compressedStart,this->buf,decompress.state_decompress);
      bytesRead=0;
      this->Update();
      return true;
    }catch(...){
      bytesRead=0;
      throw;
    }
  }

  void Reset () {bytesRead=0;decompress.reset();}
};
}
