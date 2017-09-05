#pragma once
#include<exception>
#include<string>
#include<string_view>
#include<stdio.h> //sprintf

namespace cmw{
class failure:public ::std::exception{
  ::std::string str;

public:
  inline explicit failure (char const* w):str(w){}
  inline explicit failure (::std::string_view w):str(w){}

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

  inline failure& operator<< (::std::string_view const& s){
    str.append(s);
    return *this;
  }

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

class connection_lost:public failure{
public:
  inline explicit connection_lost (char const* w):failure(w){}

  template<class T>
  connection_lost& operator<< (T t){
    failure::operator<<(t);
    return *this;
  }
};
}
