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
  marshalling_integer len;
  ::std::array<char,N> str;

 public:
  fixed_string ()=default;

  inline explicit fixed_string (char const* s):len(::strlen(s)){
    if(len()>N-1)throw failure("fixed_string ctor");
    ::strcpy(&str[0],s);
  }

  inline explicit fixed_string (::std::string_view s):len(s.length()){
    if(len()>N-1)throw failure("fixed_string ctor");
    ::strncpy(&str[0],s.data(),len());
    str[len()]='\0';
  }

  template<class R>
  explicit fixed_string (ReceiveBuffer<R>& b):len(b){
    if(len()>N-1)throw failure("fixed_string stream ctor");
    b.Give(&str[0],len());
    str[len()]='\0';
  }

  inline void Marshal (SendBuffer& b,bool=false)const{
    len.Marshal(b);
    b.Receive(&str[0],len());
  }

  inline auto bytes_available (){return N-(len()+1);}

  inline char* operator() (){return &str[0];}
  inline char const* c_str ()const{return &str[0];}
  inline char operator[] (int i)const{return str[i];}
};

using fixed_string_60=fixed_string<60>;
using fixed_string_120=fixed_string<120>;
}
