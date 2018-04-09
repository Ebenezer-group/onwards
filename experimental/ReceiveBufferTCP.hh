#pragma once

//#include<iostream>
#include"ErrorWords.hh"
#include"Buffer.hh"
#include<string.h>//memmove

namespace cmw{
template<class R>
class ReceiveBufferTCP:public ReceiveBuffer<R>{
  int bytesAvailable=0;

public:
  sock_type sock_;

  explicit ReceiveBufferTCP (int sz):ReceiveBuffer<R>(sz){}


  void BulkRead (){
    if(0==bytesAvailable) this->index=0;
    bytesAvailable+=sockRead(sock_,this->buf+bytesAvailable
                             ,this->bufsize-bytesAvailable);
  }

  bool GotPacket (){
    try{
      static bool initial=true;
      if(initial){
        this->packetLength=sizeof this->packetLength;
        if(bytesAvailable<this->packetLength){
          if(this->index>0){
            ::memmove(this->buf,this->buf+this->index,bytesAvailable);
            this->index=0;
          }
          return false;
        }
        this->packetLength=this->template Give<uint32_t>();
        this->packetLength+=sizeof this->packetLength;
        if(this->packetLength>this->bufsize){
          throw failure("ReceiveBufferTCP::GotPacket -- incoming size too great");
        }
        initial=false;
      }

      if(bytesAvailable<this->packetLength){
        if(this->index>static_cast<int32_t>(sizeof this->packetLength)){
          ::memmove(this->buf,this->buf+this->index,bytesAvailable);
          this->index=0;
        }
        return false;
      }

      bytesAvailable-=this->packetLength;
      initial=true;
      return true;
    } catch(...){
      bytesAvailable=0;
      throw;
    }
  }

  void Reset (){bytesAvailable=0;}
};
}
