#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"

#ifndef CMW_WINDOWS
#include<netdb.h>
#include<sys/socket.h>
#include<sys/types.h>
#endif

namespace cmw{
class getaddrinfo_wrapper{
  ::addrinfo* addr;

 public:
  inline getaddrinfo_wrapper (char const* node,char const* port
                              ,int socktype,int flags=0){
    ::addrinfo hints={flags,AF_UNSPEC,socktype,0,0,0,0,0};
    int rc=::getaddrinfo(node,port,&hints,&addr);
    if(rc!=0)throw failure("Getaddrinfo: ")<<gai_strerror(rc);
  }

  inline ~getaddrinfo_wrapper (){::freeaddrinfo(addr);}
  inline auto operator() (){return addr;}

  getaddrinfo_wrapper (getaddrinfo_wrapper const&)=delete;
  getaddrinfo_wrapper& operator= (getaddrinfo_wrapper)=delete;
};
}
