#pragma once
#include"IO.hh"
#include"platforms.hh"
#include"ReceiveBuffer.hh"
#include"udp_stuff.hh"

namespace cmw{

template <class R,int size=udp_packet_max>
class ReceiveBufferStack : public ReceiveBuffer<R>
{
  char ar[size];

public:

  ReceiveBufferStack (sock_type sock,::sockaddr* fromAddr=nullptr
                      , ::socklen_t* fromLen=nullptr):
    ReceiveBuffer<R>(ar,sockRead(sock,ar,size,fromAddr,fromLen))
  {
    this->NextMessage();
  }
};

}
