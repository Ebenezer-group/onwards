#pragma once
#include"close_socket.hh"
#include"ErrorWords.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"

namespace cmw{
auto const udp_packet_max=1280;

inline sock_type udp_server (char const* port){
  getaddrinfo_wrapper res(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  for(auto rp=res.get();rp!=nullptr;rp=rp->ai_next){
    auto sock=::socket(rp->ai_family,rp->ai_socktype,0);
    if(-1==sock)continue;
    if(0==::bind(sock,rp->ai_addr,rp->ai_addrlen))return sock;
    auto save=GetError();
    close_socket(sock);
    throw failure("udp_server ")<<save;
  }
  throw failure("udp_server");
}
}
