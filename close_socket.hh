#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"
#if !defined(CMW_WINDOWS)
#include<fcntl.h>
#include<unistd.h> //close
#endif

namespace cmw{
inline void close_socket (sock_type sock)
{
#ifdef CMW_WINDOWS
  if(::closesocket(sock)==SOCKET_ERROR){
    throw failure("close_socket ")<<GetError();
#else
  if(::close(sock)==-1){
    auto errval=GetError();
    if(EINTR==errval&&::close(sock)==0)return;
    throw failure("close_socket")<<errval;
#endif
  }
}

inline void set_nonblocking (sock_type sock)
{
#ifndef CMW_WINDOWS
  if(::fcntl(sock,F_SETFL,O_NONBLOCK)==-1)throw failure("set_nonb:")<<errno;
#endif
}
}
