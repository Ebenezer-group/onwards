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
      throw failure("tcp_server setsockopt ")<<GetError();
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
  auto nusock=::accept(sock,nullptr,nullptr);
  if(nusock>=0)return nusock;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept_wrapper ")<<GetError();
}

#if defined(__FreeBSD__)||defined(__linux__)
inline auto accept4_wrapper(sock_type sock,int flags)
{
  ::sockaddr amb_addr;
  ::socklen_t amblen=sizeof(amb_addr);
  auto nusock=::accept4(sock,&amb_addr,&amblen,flags);
  if(nusock>=0)return nusock;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept4_wrapper ")<<GetError();
}
#endif
}
