#pragma once

#include "platforms.hh"
#ifndef CMW_WINDOWS
#include <sys/types.h>
#include <sys/socket.h>
#endif

namespace cmw {
  int sockWrite (sock_type,void const* data,int len
                ,::sockaddr* =nullptr,::socklen_t=0);
  int sockRead (sock_type,char* data,int len
                ,::sockaddr* =nullptr,::socklen_t* =nullptr);
}

