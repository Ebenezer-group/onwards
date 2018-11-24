#pragma once
#include<exception>
#include<string>
#include<stdio.h>//sprintf
#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#define CMW_WINDOWS
#include<winsock2.h>
#include<ws2tcpip.h>
#else
#define _MSVC_LANG 0
#include<errno.h>
#endif
#if __cplusplus>=201703L||_MSVC_LANG>=201403L
#include<string_view>
#endif

namespace cmw{
class failure:public ::std::exception{
  ::std::string str;

public:
  char const* what ()const noexcept{return str.c_str();}

  explicit failure (char const* s):str(s){}
  failure (char const* s,int tot){
    if(tot>0)str.reserve(tot);
    str=s;
  }

#if __cplusplus>=201703L||_MSVC_LANG>=201403L
  explicit failure (::std::string_view s):str(s){}

  failure& operator<< (::std::string_view const& s){
    str.append(" ");
    str.append(s);
    return *this;
  }
#endif

  failure& operator<< (char const* s){
    str.append(" ");
    str.append(s);
    return *this;
  }

  failure& operator<< (char* s){return *this<<static_cast<char const*>(s);}

  failure& operator<< (int i){
    char b[10];
    ::snprintf(b,sizeof b,"%d",i);
    return *this<<b;
  }
};

struct fiasco:failure{
  explicit fiasco(char const* s):failure(s){}

  template<class T>
  fiasco& operator<< (T t){
    failure::operator<<(t);
    return *this;
  }
};

#ifdef CMW_WINDOWS
using sockType=SOCKET;
using fileType=HANDLE;
inline int GetError (){return WSAGetLastError();}
inline void winStart (){
  WSADATA w;
  int rc=WSAStartup(MAKEWORD(2,2),&w);
  if(0!=rc)throw failure("WSAStartup")<<rc;
}
#else
using sockType=int;
using fileType=int;
inline int GetError (){return errno;}
inline void winStart (){}
#endif
}
