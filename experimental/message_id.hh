#pragma once
#include<stdint.h>
#include<Buffer.hh>

namespace cmw{
template<class R> class ReceiveBuffer;

template<class T>
class message_id{
  T const value;

public:
  explicit constexpr message_id (T val):value(val){}

  template<class R>
  explicit message_id (ReceiveBuffer<R>& buf):value(buf.template Give<T>()){}

  void Marshal (SendBuffer& buf,bool=false)const{buf.Receive(value);}
  constexpr T operator() ()const{return value;}
  bool operator== (message_id rhs)const{return value==rhs();}
  bool operator== (T rhs)const{return value==rhs;}
};

using message_id8=message_id<uint8_t>;
using message_id16=message_id<uint16_t>;
using messageid_t=uint8_t;
}
