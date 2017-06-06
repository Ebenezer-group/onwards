#pragma once
#include"SendBuffer.hh"
#include"udp_stuff.hh"

namespace cmw{
template<unsigned long N=udp_packet_max>
class SendBufferStack:public SendBuffer{
  unsigned char ar[N];

public:
  SendBufferStack ():SendBuffer(ar,N){}
};
}
