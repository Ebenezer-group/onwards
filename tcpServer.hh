#pragma once
#include"ErrorWords.hh"
#include"wrappers.hh"
#include<fcntl.h>

namespace cmw{
inline sock_type tcpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_STREAM,AI_PASSIVE);
  for(auto r=res();r!=nullptr;r=r->ai_next){
    auto sock=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==sock)continue;

    int on=1;
    if(::setsockopt(sock,SOL_SOCKET,SO_REUSEADDR
                    ,(char const*)&on,sizeof(on))<0){
      closeSocket(sock);
      throw failure("tcpServer setsockopt ")<<GetError();
    }

    if(::bind(sock,r->ai_addr,r->ai_addrlen)<0){
      closeSocket(sock);
      throw failure("tcpServer bind ")<<GetError();
    }

    if(::listen(sock,SOMAXCONN)<0){
      closeSocket(sock);
      throw failure("tcpServer listen ")<<GetError();
    }

    return sock;
  }
  throw failure("tcpServer");
}

inline auto acceptWrapper(sock_type s){
  auto nu=::accept(s,nullptr,nullptr);
  if(nu>=0)return nu;

  if(ECONNABORTED==GetError())return 0;
  throw failure("acceptWrapper ")<<GetError();
}

#if defined(__FreeBSD__)||defined(__linux__)
inline auto accept4Wrapper(sock_type s,int flags){
  ::sockaddr amb;
  ::socklen_t len=sizeof(amb);
  auto nu=::accept4(s,&amb,&len,flags);
  if(nu>=0)return nu;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept4Wrapper ")<<GetError();
}
#endif
}
