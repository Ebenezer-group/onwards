#pragma once
namespace cmw{
#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#define CMW_WINDOWS
#include"ErrorWords.hh"
#include<winsock2.h>
#include<ws2tcpip.h>
using sock_type=SOCKET;
using file_type=HANDLE;
inline int GetError (){return WSAGetLastError();}
inline void windowsStart (){
  WSADATA w;
  int rc=WSAStartup(MAKEWORD(2,2),&w);
  if(0!=rc)throw failure("WSAStartup:")<<rc;
}
#else
#include<errno.h>
using sock_type=int;
using file_type=int;
inline int GetError (){return errno;}
inline void windowsStart (){}
#endif
}
