#pragma once
#include"marshalling_integer.hh"
#include"SendBuffer.hh"
#include<initializer_list>
#include<string_view>

namespace cmw{
class string_plus{
  ::std::initializer_list<::std::string_view> lst;

 public:
  inline string_plus (::std::initializer_list<::std::string_view> in):lst(in){}

  inline void Marshal (SendBuffer& buf,bool=false)const{
    int32_t t=0;
    for(auto s:lst)t+=s.length();
    marshalling_integer(t).Marshal(buf);
    for(auto s:lst)buf.Receive(s.data(),s.length());//Use low-level Receive
  }
};
}
