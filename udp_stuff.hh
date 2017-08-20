#pragma once
#include"close_socket.hh"
#include"ErrorWords.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"

namespace cmw{
auto const udp_packet_max=1280;

inline sock_type udp_server (char const* port){
  getaddrinfo_wrapper res(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  for(auto r=res.get();r!=nullptr;r=r->ai_next){
    auto sock=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==sock)continue;
    if(0==::bind(sock,r->ai_addr,r->ai_addrlen))return sock;
    auto save=GetError();
    close_socket(sock);
    throw failure("udp_server ")<<save;
  }
  throw failure("udp_server");
}
}
