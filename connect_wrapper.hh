#pragma once
#include"close_socket.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"

namespace cmw{
inline auto connect_wrapper(char const* node,char const* port){
  getaddrinfo_wrapper res(node,port,SOCK_STREAM);
  for(auto r=res.get();r!=nullptr;r=r->ai_next){
    sock_type sock=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==sock)continue;
    if(0==::connect(sock,r->ai_addr,r->ai_addrlen))return sock;
    auto save=errno;
    close_socket(sock);
    errno=save;
    return -1;
  }
  return -1;
}
}
