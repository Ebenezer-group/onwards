#pragma once

#include "ErrorWords.hh"
#include "platforms.hh"

#ifndef CMW_WINDOWS
#include <unistd.h>
#endif

namespace cmw {
#if defined(CMW_WINDOWS)
inline DWORD Write (HANDLE hndl,void const* data,int len)
{
  DWORD bytesWritten=0;
  if(!WriteFile(hndl,static_cast<char const*>(data),len
                ,&bytesWritten,nullptr)){
    throw failure("Write--WriteFile ")<<GetLastError();
  }
  return bytesWritten;
}

inline DWORD Read (HANDLE hndl,void* data,int len)
{
  DWORD bytesRead=0;
  if (!ReadFile(hndl,static_cast<char*>(data),len
                ,&bytesRead,nullptr)){
    throw failure("Read--ReadFile")<<GetLastError();
  }
  return bytesRead;
}

#else

inline int Write (int fd,void const* data,int len)
{
  int rc=::write(fd,data,len);
  if(rc>=0)return rc;
  throw failure("Write ")<<errno;
}

inline int Read (int fd,void* data,int len)
{
  int rc=::read(fd,data,len);
  if(rc>0)return rc;
  if(rc==0)throw connection_lost("Read--eof len: ")<<len;
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  throw failure("Read -- len: ")<<len<<" "<<errno;
}
#endif
}
