#pragma once
#include<exception>
#include<string>
#include<string_view>
#include<stdio.h> //sprintf

namespace cmw{
class failure:public ::std::exception{
  ::std::string whatStr;

public:
  inline explicit failure (char const* w):whatStr(w){}
  inline explicit failure (::std::string_view w):whatStr(w){}

  inline failure (char const* w,int tot){
    if(tot>0)whatStr.reserve(tot);
    whatStr=w;
  }

  inline char const* what ()const noexcept {return whatStr.c_str();}
  //::std::string_view what_view ()const noexcept
  //{return ::std::string_view(whatStr);}

  inline failure& operator<< (::std::string const& s){
    whatStr.append(s);
    return *this;
  }

  inline failure& operator<< (::std::string_view const& s){
    whatStr.append(s);
    return *this;
  }

  inline failure& operator<< (char* s){
    whatStr.append(s);
    return *this;
  }

  inline failure& operator<< (char const* s){
    whatStr.append(s);
    return *this;
  }

  inline failure& operator<< (int i){
    char buf[20];
    ::sprintf(buf,"%d",i);
    return *this<<buf;
  }

  template<class T>
  failure& operator<< (T val){
    using ::std::to_string;
    return *this<<to_string(val);
  }
};

class connection_lost:public failure{
public:
  inline explicit connection_lost (char const* w):failure(w){}

  template<class T>
  connection_lost& operator<< (T val){
    failure::operator<<(val);
    return *this;
  }
};
}
