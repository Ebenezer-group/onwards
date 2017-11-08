#pragma once
#include<stdint.h>
#include<stdlib.h>//strtol

namespace cmw{
class SendBuffer;
template<class R> class ReceiveBuffer;

class marshalling_integer{
  int32_t val;

public:
  inline marshalling_integer (){}
  inline explicit marshalling_integer (int32_t v):val(v){}
  inline explicit marshalling_integer (char const* v):val(::strtol(v,0,10)){}

  //Reads a sequence of bytes in variable-length format and
  //composes a 32 bit integer.
  template<class R>
  explicit marshalling_integer (ReceiveBuffer<R>& b):val(0){
    uint32_t shift=1;
    for(;;){
      uint8_t a=b.GiveOne();
      val+= (a&127)*shift;
      if((a&128)==0)return;
      shift<<=7;
      val+=shift;
    }
  }

  inline void operator= (int32_t r){val=r;}
  inline auto operator() ()const{return val;}
  inline bool operator== (marshalling_integer r)const{return val==r();}
  inline bool operator== (int32_t r)const{return val==r;}
  void Marshal (SendBuffer&,bool=false)const;
};
}
