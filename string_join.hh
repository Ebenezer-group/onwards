#pragma once

#include "marshalling_integer.hh"
#include <experimental/string_view>
#include <string.h>

namespace cmw {

class string_join
{
  // hand_written_marshalling_code
  char const* s1;
  char const* s2;
  int len1;
  int len2;

 public:
  string_join (char const* str1,char const* str2) : s1(str1),s2(str2)
                             ,len1(::strlen(s1)),len2(::strlen(s2))
  {}

  string_join (char const* str1,::std::experimental::string_view str2) : s1(str1),s2(str2.data())
                             ,len1(::strlen(s1)),len2(str2.size())
  {}

  void Marshal (::cmw::SendBuffer& buf,bool=false) const
  {
    ::cmw::marshalling_integer(len1+len2).Marshal(buf);
    buf.Receive(s1,len1);
    buf.Receive(s2,len2);
  }
};

}
