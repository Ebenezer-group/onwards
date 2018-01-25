#pragma once
#include<exception>
#include<string>
#if __cplusplus>=201703L
#include<string_view>
#endif
#include<stdio.h>//sprintf
#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#define CMW_WINDOWS
#include<winsock2.h>
#include<ws2tcpip.h>
#else
#include<errno.h>
#endif

namespace cmw{
class failure:public ::std::exception{
  ::std::string str;

public:
  inline explicit failure (char const* w):str(w){}
#if __cplusplus>=201703L
  inline explicit failure (::std::string_view w):str(w){}
#endif

  inline failure (char const* w,int tot){
    if(tot>0)str.reserve(tot);
    str=w;
  }

  inline char const* what ()const noexcept{return str.c_str();}
  //::std::string_view what_view ()const noexcept
  //{return ::std::string_view(str);}

  inline failure& operator<< (::std::string const& s){
    str.append(s);
    return *this;
  }

#if __cplusplus>=201703L
  inline failure& operator<< (::std::string_view const& s){
    str.append(s);
    return *this;
  }
#endif

  inline failure& operator<< (char* s){
    str.append(s);
    return *this;
  }

  inline failure& operator<< (char const* s){
    str.append(s);
    return *this;
  }

  inline failure& operator<< (int i){
    char b[20];
    ::sprintf(b,"%d",i);
    return *this<<b;
  }

  template<class T>
  failure& operator<< (T t){
    using ::std::to_string;
    return *this<<to_string(t);
  }
};

class connectionLost:public failure{
public:
  inline explicit connectionLost (char const* w):failure(w){}

  template<class T>
  connectionLost& operator<< (T t){
    failure::operator<<(t);
    return *this;
  }
};

#ifdef CMW_WINDOWS
using sockType=SOCKET;
using fileType=HANDLE;
inline int GetError (){return WSAGetLastError();}
inline void windowsStart (){
  WSADATA w;
  int rc=WSAStartup(MAKEWORD(2,2),&w);
  if(0!=rc)throw failure("WSAStartup:")<<rc;
}
#else
using sockType=int;
using fileType=int;
inline int GetError (){return errno;}
inline void windowsStart (){}
#endif
}
