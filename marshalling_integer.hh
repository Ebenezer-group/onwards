#pragma once

#include <stdint.h>
#include <stdlib.h> // strtol

namespace cmw {
class SendBuffer;
template <class R> class ReceiveBuffer;

class marshalling_integer
{
  int32_t value;

public:
  marshalling_integer () {}
  explicit marshalling_integer (int32_t val) : value(val) {}
  explicit marshalling_integer (char const* val) : value(::strtol(val,0,10)) {}

  // Reads a sequence of byte values in variable-
  // length format and composes a 32 bit integer.
  template <class R>
  explicit marshalling_integer (ReceiveBuffer<R>& buf) : value(0)
  {
    uint32_t shift=1;
    for(;;){
      uint8_t abyte=buf.GiveOne();
      value += (abyte&127)*shift;
      if((abyte&128)==0)break;

      shift<<=7;
      value+=shift;
    }
  }

  int32_t operator() () const { return value; }
  void operator= (int32_t rhs) { value=rhs; }

  bool operator== (marshalling_integer const& rhs) const
  { return value==rhs(); }

  bool operator== (int32_t rhs) const { return value==rhs; }

  bool operator> (int32_t rhs) const { return value>rhs; }
  void operator-= (int32_t rhs) { value-=rhs; }

  //void CalculateMarshallingSize (Counter&) const;
  void Marshal (SendBuffer& buf,bool=false) const;
};
}
