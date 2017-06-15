#pragma once
#include"marshalling_integer.hh"
#include"SendBuffer.hh"
#include<initializer_list>
#include<string_view>

namespace cmw{
class string_join{
  // hand_written_marshalling_code
  ::std::initializer_list<::std::string_view> in;

 public:
  string_join (::std::initializer_list<::std::string_view> lst):in(lst){}

  void Marshal (SendBuffer& buf,bool=false)const{
    int totLen=0;
    for(auto sv:in)totLen+=sv.length();
    marshalling_integer(totLen).Marshal(buf);
    for(auto sv:in)buf.Receive(sv.data(),sv.length());//Use low-level Receive
  }
};
}
