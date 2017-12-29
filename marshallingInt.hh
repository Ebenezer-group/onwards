#pragma once
#include<stdint.h>
#include<stdlib.h>//strtol

namespace cmw{
class SendBuffer;
template<class R> class ReceiveBuffer;

class marshallingInt{
  int32_t val;

public:
  inline marshallingInt (){}
  inline explicit marshallingInt (int32_t v):val(v){}
  inline explicit marshallingInt (char const* v):val(::strtol(v,0,10)){}

  //Reads a sequence of bytes in variable-length format and
  //composes a 32 bit integer.
  template<class R>
  explicit marshallingInt (ReceiveBuffer<R>& b):val(0){
    uint32_t shift=1;
    for(;;){
      uint8_t a=b.GiveOne();
      val+=(a&127)*shift;
      if((a&128)==0)return;
      shift<<=7;
      val+=shift;
    }
  }

  inline void operator= (int32_t r){val=r;}
  inline auto operator() ()const{return val;}
  void Marshal (SendBuffer&,bool=false)const;
};

inline bool operator== (marshallingInt l,marshallingInt r){return l()==r();}
inline bool operator== (marshallingInt l,int32_t r){return l()==r;}
}
