#pragma once

#include "marshalling_integer.hh"
#include "SendBuffer.hh"
#include <experimental/string_view>

namespace cmw {

class string_join
{
  // hand_written_marshalling_code
  ::std::experimental::string_view s1;
  ::std::experimental::string_view s2;

 public:
  string_join (::std::experimental::string_view str1
	       ,::std::experimental::string_view str2) : s1(str1),s2(str2)
  {}

  void Marshal (::cmw::SendBuffer& buf,bool=false) const
  {
    ::cmw::marshalling_integer(s1.length()+s2.length()).Marshal(buf);
    buf.Receive(s1);
    buf.Receive(s2);
  }
};

}
