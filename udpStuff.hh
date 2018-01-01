#pragma once
#include"ErrorWords.hh"
#include"wrappers.hh"

namespace cmw{
auto const udp_packet_max=1280;

inline sock_type udpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  for(auto r=res();r!=nullptr;r=r->ai_next){
    auto s=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==s)continue;
    if(0==::bind(s,r->ai_addr,r->ai_addrlen))return s;
    auto v=GetError();
    closeSocket(s);
    throw failure("udpServer ")<<v;
  }
  throw failure("udpServer");
}
}
