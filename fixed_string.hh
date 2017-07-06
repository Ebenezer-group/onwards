#pragma once
#include"ErrorWords.hh"
#include"marshalling_integer.hh"
#include"SendBuffer.hh"
#include<array>
#include<string_view>
#include<string.h>

namespace cmw{
template<int N>
class fixed_string{
  int length;
  ::std::array<char,N> str;

 public:
  explicit fixed_string (char const* s):length(::strlen(s)){
    if(length>N-1)throw failure("fixed_string ctor");
    ::strcpy(&str[0],s);
  }

  explicit fixed_string (::std::string_view s):length(s.length()){
    if(length>N-1)throw failure("fixed_string ctor");
    ::strncpy(&str[0],s.data(),length);
    str[length]='\0';
  }

  template<class R>
  explicit fixed_string (ReceiveBuffer<R>& buf):length(marshalling_integer(buf)()){
    if(length>N-1)throw failure("fixed_string stream ctor");
    buf.Give(&str[0],length);
    str[length]='\0';
  }

  void Marshal (SendBuffer& buf,bool=false)const{
    marshalling_integer(length).Marshal(buf);
    buf.Receive(&str[0],length);
  }

  char const* c_str ()const{return &str[0];}
  char operator[] (int index)const{return str[index];}
};

using fixed_string_60=fixed_string<60>;
using fixed_string_120=fixed_string<120>;
}
