#pragma once

#include"close_socket.hh"
#include"ErrorWords.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"

namespace cmw {

inline sock_type connect_wrapper(char const* node,char const* port)
{
  getaddrinfo_wrapper res(node,port,SOCK_STREAM);
  for(auto rp=res.get(); rp!=nullptr; rp=rp->ai_next){
    auto sock=::socket(rp->ai_family,rp->ai_socktype,0);
    if(-1==sock)continue;
    if(0==::connect(sock,rp->ai_addr,rp->ai_addrlen))return sock;
    failure f("connect_wrapper node: ");
    f<<node<<" "<<GetError();
    close_socket(sock);
    throw f;
  }
  throw failure("connect_wrapper");
}
}
