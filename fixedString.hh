#pragma once
#include"Buffer.hh"
#include"ErrorWords.hh"
#include"marshallingInt.hh"
#include<array>
#include<string_view>
#include<string.h>

namespace cmw{
template<int N>
class fixedString{
  marshallingInt len;
  ::std::array<char,N> str;

 public:
  fixedString ()=default;

  inline explicit fixedString (char const* s):len(::strlen(s)){
    if(len()>N-1)throw failure("fixedString ctor");
    ::strcpy(&str[0],s);
  }

  inline explicit fixedString (::std::string_view s):len(s.length()){
    if(len()>N-1)throw failure("fixedString ctor");
    ::strncpy(&str[0],s.data(),len());
    str[len()]='\0';
  }

  template<class R>
  explicit fixedString (ReceiveBuffer<R>& b):len(b){
    if(len()>N-1)throw failure("fixedString stream ctor");
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

using fixedString60=fixedString<60>;
using fixedString120=fixedString<120>;
}
