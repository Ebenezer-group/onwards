#pragma once
#include"marshalling_integer.hh"
#include"SendBuffer.hh"
#include<initializer_list>
#include<string_view>

namespace cmw{
class string_plus{
  ::std::initializer_list<::std::string_view> lst;

 public:
  string_plus (::std::initializer_list<::std::string_view> in):lst(in){}

  void Marshal (SendBuffer& buf,bool=false)const{
    int32_t totLen=0;
    for(auto sv:lst)totLen+=sv.length();
    marshalling_integer(totLen).Marshal(buf);
    for(auto sv:lst)buf.Receive(sv.data(),sv.length());//Use low-level Receive
  }
};
}
