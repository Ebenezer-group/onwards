#pragma once
#include"ErrorWords.hh"
#if __cplusplus>=201703L||_MSVC_LANG>=201403L
#include<charconv>//from_chars
#endif
#include<stdio.h>
#include<stdlib.h>//strtol,exit

#ifdef CMW_WINDOWS
#define poll WSAPoll
#define LOG_INFO 0
#define LOG_ERR 0
#else
#include<fcntl.h>//open
#include<netdb.h>
#include<sys/socket.h>
#include<sys/stat.h>//open
#include<sys/types.h>
#include<unistd.h>//close,chdir,read,write
#include<poll.h>
#include<stdarg.h>
#include<syslog.h>
#endif

namespace cmw{
inline int fromChars (char const* p){
#if __cplusplus>=201703L||_MSVC_LANG>=201403L
  int res=0;
  ::std::from_chars(p,p+::strlen(p),res);
  return res;
#else
  return ::strtol(p,0,10);
#endif
}

#ifndef CMW_WINDOWS
struct fileWrapper{
  int const d;

  fileWrapper (char const* name,int flags,mode_t mode=0):
	  d(0==mode?::open(name,flags): ::open(name,flags,mode)){
    if(d<0)throw failure("fileWrapper ")<<name<<" "<<errno;
  }
  ~fileWrapper (){::close(d);}
};
#endif

struct FILE_wrapper{
  FILE* hndl;
  char line[120];

  FILE_wrapper (char const* fn,char const* mode){
    if((hndl=::fopen(fn,mode))==nullptr)
      throw failure("FILE_wrapper ")<<fn<<" "<<mode<<" "<<GetError();
  }
  char* fgets (){return ::fgets(line,sizeof line,hndl);}
  ~FILE_wrapper (){::fclose(hndl);}
};

template<typename... T>
void syslogWrapper (int priority,char const* format,T... t){
#ifndef CMW_WINDOWS
  ::syslog(priority,format,t...);
#endif
}

template<typename... T>
void bail (char const* fmt,T... t)noexcept{
  syslogWrapper(LOG_ERR,fmt,t...);
  ::exit(EXIT_FAILURE);
}

inline void setDirectory (char const* d){
#ifdef CMW_WINDOWS
  if(!SetCurrentDirectory(d))
#else
  if(::chdir(d)==-1)
#endif
    throw failure("setDirectory ")<<d<<" "<<GetError();
}

inline void closeSocket (sockType s){
#ifdef CMW_WINDOWS
  if(::closesocket(s)==SOCKET_ERROR)
#else
  if(::close(s)==-1)
#endif
    throw failure("closeSocket ")<<GetError();
}

inline int preserveError (sockType s){
  auto e=GetError();
  closeSocket(s);
  return e;
}

class getaddrinfoWrapper{
  ::addrinfo* head;
  ::addrinfo* addr;

 public:
  getaddrinfoWrapper (char const* node,char const* port
                      ,int socktype,int flags=0){
    ::addrinfo hints{flags,AF_UNSPEC,socktype,0,0,0,0,0};
    int rc=::getaddrinfo(node,port,&hints,&head);
    if(rc!=0)throw failure("getaddrinfo ")<<::gai_strerror(rc);
    addr=head;
  }

  ~getaddrinfoWrapper (){::freeaddrinfo(head);}
  ::addrinfo* operator() (){return addr;}
  void inc (){if(addr!=nullptr)addr=addr->ai_next;}
  sockType getSock (){
    for(;addr!=nullptr;addr=addr->ai_next){
      auto s=::socket(addr->ai_family,addr->ai_socktype,0);
      if(-1!=s)return s;
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
  errno=preserveError(s);
  return -1;
}

inline int setNonblocking (sockType s){
#ifndef CMW_WINDOWS
  return ::fcntl(s,F_SETFL,O_NONBLOCK);
#endif
}

inline int pollWrapper (::pollfd* fds,int num,int timeout=-1){
  int rc=::poll(fds,num,timeout);
  if(rc>=0)return rc;
  throw failure("poll ")<<GetError();
}

inline void setRcvTimeout (sockType s,int time){
#ifdef CMW_WINDOWS
  DWORD t=time*1000;
#else
  struct timeval t{time,0};
#endif
  if(::setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char*)&t,sizeof t)!=0)
    throw failure("setRcvTimeout ")<<GetError();
}

inline sockType udpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  auto s=res.getSock();
  if(0==::bind(s,res()->ai_addr,res()->ai_addrlen))return s;
  auto e=preserveError(s);
  throw failure("udpServer ")<<e;
}

inline sockType tcpServer (char const* port){
  getaddrinfoWrapper res(nullptr,port,SOCK_STREAM,AI_PASSIVE);
#if 0
  res.inc();
#endif
  auto s=res.getSock();

  int on=1;
  if(::setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(char*)&on,sizeof on)<0){
    auto e=preserveError(s);
    throw failure("tcpServer setsockopt ")<<e;
  }
  if(::bind(s,res()->ai_addr,res()->ai_addrlen)<0){
    auto e=preserveError(s);
    throw failure("tcpServer bind ")<<e;
  }
  if(::listen(s,SOMAXCONN)<0){
    auto e=preserveError(s);
    throw failure("tcpServer listen ")<<e;
  }
  return s;
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
  ::socklen_t len=sizeof amb;
  int nu=::accept4(s,&amb,&len,flags);
  if(nu>=0)return nu;
  if(ECONNABORTED==GetError())return 0;
  throw failure("accept4Wrapper ")<<GetError();
}
#endif

inline int sockWrite (sockType s,void const* data,int len
                      ,sockaddr const* addr=nullptr,socklen_t toLen=0){
  int rc=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);
  if(rc>0)return rc;
  auto err=GetError();
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockWrite:")<<s<<" "<<err;}

inline int sockRead (sockType s,void* data,int len
                     ,sockaddr* addr=nullptr,socklen_t* fromLen=nullptr){
  int rc=::recvfrom(s,static_cast<char*>(data),len,0,addr,fromLen);
  if(rc>0)return rc;
  if(rc==0)throw fiasco("sockRead eof:")<<s<<" "<<len;
  auto err=GetError();
  if(ECONNRESET==err)throw fiasco("sockRead-ECONNRESET");
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockRead:")<<s<<" "<<len<<" "<<err;
}

#ifdef CMW_WINDOWS
inline DWORD Write (HANDLE h,void const* data,int len){
  DWORD bytesWritten=0;
  if(!WriteFile(h,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    throw failure("Write ")<<GetLastError();
  return bytesWritten;
}

inline DWORD Read (HANDLE h,void* data,int len){
  DWORD bytesRead=0;
  if (!ReadFile(h,static_cast<char*>(data),len,&bytesRead,nullptr))
    throw failure("Read ")<<GetLastError();
  return bytesRead;
}
#else
inline int Write (int fd,void const* data,int len){
  int rc=::write(fd,data,len);
  if(rc>=0)return rc;
  throw failure("Write ")<<errno;
}

inline int Read (int fd,void* data,int len){
  int rc=::read(fd,data,len);
  if(rc>0)return rc;
  if(rc==0)throw fiasco("Read eof:")<<len;
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  throw failure("Read:")<<len<<" "<<errno;
}
#endif
}
