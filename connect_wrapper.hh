#pragma once
#include"closeSocket.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"

namespace cmw{
inline auto connect_wrapper(char const* node,char const* port){
  getaddrinfo_wrapper res(node,port,SOCK_STREAM);
  for(auto r=res();r!=nullptr;r=r->ai_next){
    sock_type s=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==s)continue;
    if(0==::connect(s,r->ai_addr,r->ai_addrlen))return s;
    auto e=errno;
    closeSocket(s);
    errno=e;
    return -1;
  }
  return -1;
}
}
