#pragma once
#include"ErrorWords.hh"
#include"wrappers.hh"

namespace cmw{
auto const udp_packet_max=1280;

inline sock_type udpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  auto s=res.getSock();
  if(0==::bind(s,res()->ai_addr,res()->ai_addrlen))return s;
  auto e=GetError();
  closeSocket(s);
  throw failure("udpServer ")<<e;
}
}
