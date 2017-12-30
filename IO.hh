#pragma once
#include"ErrorWords.hh"
#ifndef CMW_WINDOWS
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#endif

namespace cmw{
inline int sockWrite (sock_type s,void const* data,int len
                      ,sockaddr* addr=nullptr,socklen_t toLen=0){
  int rc=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);
  if(rc>0)return rc;
  auto err=GetError();
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockWrite sock:")<<s<<" "<<err;}

inline int sockRead (sock_type s,char* data,int len
                     ,sockaddr* addr=nullptr,socklen_t* fromLen=nullptr){
  int rc=::recvfrom(s,data,len,0,addr,fromLen);
  if(rc>0)return rc;
  if(rc==0)throw connectionLost("sockRead eof sock:")<<s<<" len:"<<len;
  auto err=GetError();
  if(ECONNRESET==err)throw connectionLost("sockRead-ECONNRESET");
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockRead sock:")<<s<<" len:"<<len<<" "<<err;
}

#ifdef CMW_WINDOWS
inline auto Write (HANDLE h,void const* data,int len){
  DWORD bytesWritten=0;
  if(!WriteFile(h,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    throw failure("Write ")<<GetLastError();
  return bytesWritten;
}

inline auto Read (HANDLE h,void* data,int len){
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
  if(rc==0)throw connectionLost("Read--eof len: ")<<len;
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  throw failure("Read -- len:")<<len<<" "<<errno;
}
#endif
}
