#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"
#ifdef CMW_WINDOWS
#define poll WSAPoll
#else
#include<poll.h>
#endif

namespace cmw{
inline int poll_wrapper (::pollfd* fds,int num,int timeout=-1){
  int rc=::poll(fds,num,timeout);
  if(rc>=0)return rc;
  throw failure("poll ")<<GetError();
}
}
