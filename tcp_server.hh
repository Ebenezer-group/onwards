#pragma once

#include"close_socket.hh"
#include"ErrorWords.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"
#include<fcntl.h>

namespace cmw {

inline auto tcp_server (char const* port)
{
  getaddrinfo_wrapper res(nullptr,port,SOCK_STREAM,AI_PASSIVE);
  for(auto rp=res.get();rp!=nullptr;rp=rp->ai_next){
    sock_type sock=::socket(rp->ai_family,rp->ai_socktype,0);
    if(-1==sock)continue;

    int on=1;
    if(::setsockopt(sock,SOL_SOCKET,SO_REUSEADDR
                     ,(char const*)&on,sizeof(on))<0){
      close_socket(sock);
      throw failure("tcp_server setsockopt: ")<<GetError();
    }

    if(::bind(sock,rp->ai_addr,rp->ai_addrlen)<0){
      close_socket(sock);
      throw failure("tcp_server bind ")<<GetError();
    }

    if(::listen(sock,SOMAXCONN)<0){
      close_socket(sock);
      throw failure("tcp_server listen ")<<GetError();
    }

    return sock;
  }
  throw failure("tcp_server");
}

inline auto accept_wrapper(sock_type sock)
{
  ::sockaddr amb_addr;
  ::socklen_t amblen=sizeof(amb_addr);
  sock_type nusock=::accept(sock,&amb_addr,&amblen);
  if(nusock>=0)return nusock;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept_wrapper ")<<GetError();
}
}
