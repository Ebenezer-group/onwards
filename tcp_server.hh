#pragma once
#include"close_socket.hh"
#include"ErrorWords.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"
#include<fcntl.h>

namespace cmw{
inline auto tcp_server (char const* port){
  getaddrinfo_wrapper res(nullptr,port,SOCK_STREAM,AI_PASSIVE);
  for(auto r=res.get();r!=nullptr;r=r->ai_next){
    sock_type sock=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==sock)continue;

    int on=1;
    if(::setsockopt(sock,SOL_SOCKET,SO_REUSEADDR
                    ,(char const*)&on,sizeof(on))<0){
      close_socket(sock);
      throw failure("tcp_server setsockopt ")<<GetError();
    }

    if(::bind(sock,r->ai_addr,r->ai_addrlen)<0){
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

inline auto accept_wrapper(sock_type sock){
  auto nu=::accept(sock,nullptr,nullptr);
  if(nu>=0)return nu;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept_wrapper ")<<GetError();
}

#if defined(__FreeBSD__)||defined(__linux__)
inline auto accept4_wrapper(sock_type sock,int flags){
  ::sockaddr amb_addr;
  ::socklen_t amblen=sizeof(amb_addr);
  auto nu=::accept4(sock,&amb_addr,&amblen,flags);
  if(nu>=0)return nu;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept4_wrapper ")<<GetError();
}
#endif
}
