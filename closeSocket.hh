#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"
#ifndef CMW_WINDOWS
#include<fcntl.h>
#include<unistd.h>//close
#endif

namespace cmw{
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

inline void setNonblocking (sock_type s){
#ifndef CMW_WINDOWS
  if(::fcntl(s,F_SETFL,O_NONBLOCK)==-1)throw failure("setNonb:")<<errno;
#endif
}
}
