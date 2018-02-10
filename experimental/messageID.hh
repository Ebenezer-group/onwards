#pragma once
#include<stdint.h>
#include<Buffer.hh>

namespace cmw{
template<class R> class ReceiveBuffer;

template<class T>
class messageID{
  T const value;

public:
  explicit constexpr messageID (T val):value(val){}

  template<class R>
  explicit messageID (ReceiveBuffer<R>& buf):value(Give<T>(buf)){}

  void Marshal (SendBuffer& buf,bool=false)const{buf.Receive(value);}
  constexpr T operator() ()const{return value;}
  bool operator== (messageID rhs)const{return value==rhs();}
  bool operator== (T rhs)const{return value==rhs;}
};

using messageID8=messageID<uint8_t>;
using messageID16=messageID<uint16_t>;
using messageID_t=uint8_t;
}
