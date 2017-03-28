#pragma once

#include"ErrorWords.hh"
#include"platforms.hh"
#ifndef CMW_WINDOWS
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#endif

namespace cmw {
inline int sockWrite (sock_type sock,void const* data,int len
                      ,sockaddr* toAddr=nullptr,socklen_t toLen=0)
{
  int rc=::sendto(sock,static_cast<char const*>(data),len,0
                  ,toAddr,toLen);
  if(rc>0)return rc;
  auto err=GetError();
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockWrite sock==")<<sock<<" "<<err;
}

inline int sockRead (sock_type sock,char* data,int len
                     ,sockaddr* fromAddr=nullptr,socklen_t* fromLen=nullptr)
{
  int rc=::recvfrom(sock,data,len,0,fromAddr,fromLen);
  if(rc>0)return rc;
  if(rc==0)throw connection_lost("sockRead eof sock==")<<sock<<" len=="<<len;
  auto err=GetError();
  if(ECONNRESET==err)throw connection_lost("sockRead-ECONNRESET");
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockRead sock==")<<sock<<" len=="<<len<<" "<<err;
}

#if defined(CMW_WINDOWS)
inline DWORD Write (HANDLE hndl,void const* data,int len)
{
  DWORD bytesWritten=0;
  if(!WriteFile(hndl,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    throw failure("Write--WriteFile ")<<GetLastError();
  return bytesWritten;
}

inline DWORD Read (HANDLE hndl,void* data,int len)
{
  DWORD bytesRead=0;
  if (!ReadFile(hndl,static_cast<char*>(data),len,&bytesRead,nullptr))
    throw failure("Read--ReadFile")<<GetLastError();
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
