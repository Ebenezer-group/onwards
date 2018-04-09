#pragma once
#include"ErrorWords.hh"
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
struct FILE_wrapper{
  FILE* hndl;
  char line[120];

  inline FILE_wrapper (char const* fn,char const* mode){
    if((hndl=::fopen(fn,mode))==nullptr)
      throw failure("FILE_wrapper ")<<fn<<" "<<mode<<" "<<GetError();
  }

  inline char* fgets (){return ::fgets(line,sizeof(line),hndl);}
  inline ~FILE_wrapper (){::fclose(hndl);}
};

inline void closeSocket (sockType s){
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

class getaddrinfoWrapper{
  ::addrinfo* addr;

 public:
  inline getaddrinfoWrapper (char const* node,char const* port
                             ,int socktype,int flags=0){
    ::addrinfo hints={flags,AF_UNSPEC,socktype,0,0,0,0,0};
    int rc=::getaddrinfo(node,port,&hints,&addr);
    if(rc!=0)throw failure("getaddrinfo ")<<gai_strerror(rc);
  }

  inline ~getaddrinfoWrapper (){::freeaddrinfo(addr);}
  inline ::addrinfo* operator() (){return addr;}
  inline sockType getSock (){
    for(;addr!=nullptr;addr=addr->ai_next){
      auto s=::socket(addr->ai_family,addr->ai_socktype,0);
      if(-1==s)continue;
      return s;
    }
    throw failure("getaddrinfo getSock");
  }

  getaddrinfoWrapper (getaddrinfoWrapper const&)=delete;
  getaddrinfoWrapper& operator= (getaddrinfoWrapper)=delete;
};

inline sockType connectWrapper (char const* node,char const* port){
  getaddrinfoWrapper res(node,port,SOCK_STREAM);
  auto s=res.getSock();
  if(0==::connect(s,res()->ai_addr,res()->ai_addrlen))return s;
  auto e=errno;
  closeSocket(s);
  errno=e;
  return -1;
}

inline int pollWrapper (::pollfd* fds,int num,int timeout=-1){
  int rc=::poll(fds,num,timeout);
  if(rc>=0)return rc;
  throw failure("poll ")<<GetError();
}

template<typename... T>
void syslogWrapper (int priority,char const* format,T... t){
#ifndef CMW_WINDOWS
  ::syslog(priority,format,t...);
#endif
}

inline void setNonblocking (sockType s){
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

inline sockType tcpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_STREAM,AI_PASSIVE);
#if 0
  auto r=res();
  r=r->ai_next;
  for(;r!=nullptr;r=r->ai_next){
#else
  for(auto r=res();r!=nullptr;r=r->ai_next){
#endif
    auto s=::socket(r->ai_family,r->ai_socktype,0);
    if(-1==s)continue;

    int on=1;
    if(::setsockopt(s,SOL_SOCKET,SO_REUSEADDR
                    ,(char const*)&on,sizeof(on))<0){
      auto e=GetError();
      closeSocket(s);
      throw failure("tcpServer setsockopt ")<<e;
    }

    if(::bind(s,r->ai_addr,r->ai_addrlen)<0){
      auto e=GetError();
      closeSocket(s);
      throw failure("tcpServer bind ")<<e;
    }

    if(::listen(s,SOMAXCONN)<0){
      auto e=GetError();
      closeSocket(s);
      throw failure("tcpServer listen ")<<e;
    }

    return s;
  }
  throw failure("tcpServer");
}

inline int acceptWrapper(sockType s){
  int nu=::accept(s,nullptr,nullptr);
  if(nu>=0)return nu;

  if(ECONNABORTED==GetError())return 0;
  throw failure("acceptWrapper ")<<GetError();
}

#if defined(__FreeBSD__)||defined(__linux__)
inline int accept4Wrapper(sockType s,int flags){
  ::sockaddr amb;
  ::socklen_t len=sizeof(amb);
  int nu=::accept4(s,&amb,&len,flags);
  if(nu>=0)return nu;

  if(ECONNABORTED==GetError())return 0;
  throw failure("accept4Wrapper ")<<GetError();
}
#endif

inline sockType udpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  auto s=res.getSock();
  if(0==::bind(s,res()->ai_addr,res()->ai_addrlen))return s;
  auto e=GetError();
  closeSocket(s);
  throw failure("udpServer ")<<e;
}
}
