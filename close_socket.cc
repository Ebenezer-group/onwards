#include "close_socket.hh"

#if !defined(CMW_WINDOWS)
#include <unistd.h> // close
#endif

#include "ErrorWords.hh"

void cmw::close_socket (sock_type sock)
{
#ifdef CMW_WINDOWS
  if(::closesocket(sock)==SOCKET_ERROR){
    throw failure("close_socket failed ")<<GetError();
#else
  if(::close(sock)==-1){
    auto errval=GetError();
    if(EINTR==errval){
      if(close(sock)==0){
        return;
      }
    }
    throw failure("close_socket failed ")<<errval;
#endif
  }
}
