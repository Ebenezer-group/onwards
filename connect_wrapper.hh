#pragma once
#include"close_socket.hh"
#include"getaddrinfo_wrapper.hh"
#include"platforms.hh"

namespace cmw{
inline auto connect_wrapper(char const* node,char const* port){
  getaddrinfo_wrapper res(node,port,SOCK_STREAM);
  for(auto r=res.get();r!=nullptr;r=r->ai_next){
    sock_type s=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==s)continue;
    if(0==::connect(s,r->ai_addr,r->ai_addrlen))return s;
    auto e=errno;
    close_socket(s);
    errno=e;
    return -1;
  }
  return -1;
}
}
