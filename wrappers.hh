#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"
#include<stdio.h>

#ifdef CMW_WINDOWS
#define poll WSAPoll
#define LOG_INFO 0
#define LOG_ERR 0
#else
#include<fcntl.h>
#include<netdb.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>//close,chdir
#include<poll.h>
#include<stdarg.h>
#include<syslog.h>
#endif

namespace cmw{
class FILE_wrapper{
public:
  FILE* hndl;

  inline FILE_wrapper (char const* fn,char const* mode){
    if((hndl=::fopen(fn,mode))==nullptr)
      throw failure("FILE_wrapper ")<<fn<<" "<<mode<<" "<<GetError();
  }

  inline ~FILE_wrapper (){::fclose(hndl);}
};

class getaddrinfo_wrapper{
  ::addrinfo* addr;

 public:
  inline getaddrinfo_wrapper (char const* node,char const* port
                              ,int socktype,int flags=0){
    ::addrinfo hints={flags,AF_UNSPEC,socktype,0,0,0,0,0};
    int rc=::getaddrinfo(node,port,&hints,&addr);
    if(rc!=0)throw failure("Getaddrinfo ")<<gai_strerror(rc);
  }

  inline ~getaddrinfo_wrapper (){::freeaddrinfo(addr);}
  inline auto operator() (){return addr;}

  getaddrinfo_wrapper (getaddrinfo_wrapper const&)=delete;
  getaddrinfo_wrapper& operator= (getaddrinfo_wrapper)=delete;
};

inline void closeSocket (sock_type s){
#ifdef CMW_WINDOWS
  if(::closesocket(s)==SOCKET_ERROR){
    throw failure("closeSocket ")<<GetError();
#else
  if(::close(s)==-1){
    auto err=errno;
    if(EINTR==err&&::close(s)==0)return;
    throw failure("closeSocket")<<err;
#endif
  }
}

inline sock_type connect_wrapper(char const* node,char const* port){
  getaddrinfo_wrapper res(node,port,SOCK_STREAM);
  for(auto r=res();r!=nullptr;r=r->ai_next){
    auto s=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==s)continue;
    if(0==::connect(s,r->ai_addr,r->ai_addrlen))return s;
    auto e=errno;
    closeSocket(s);
    errno=e;
    return -1;
  }
  return -1;
}

inline int poll_wrapper (::pollfd* fds,int num,int timeout=-1){
  int rc=::poll(fds,num,timeout);
  if(rc>=0)return rc;
  throw failure("poll ")<<GetError();
}

template<typename... T>
void syslog_wrapper (int priority,char const* format,T... t){
#ifndef CMW_WINDOWS
  ::syslog(priority,format,t...);
#endif
}

inline void setNonblocking (sock_type s){
#ifndef CMW_WINDOWS
  if(::fcntl(s,F_SETFL,O_NONBLOCK)==-1)throw failure("setNonb:")<<errno;
#endif
}

inline void setDirectory (char const* d){
#ifdef CMW_WINDOWS
  if(!SetCurrentDirectory(d))
#else
  if(::chdir(d)==-1)
#endif
    throw failure("setDirectory ")<<d<<" "<<GetError();
}
}
