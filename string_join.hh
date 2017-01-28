#pragma once

#include "marshalling_integer.hh"
#include "SendBuffer.hh"
#include <string_view>

namespace cmw {

class string_join
{
  // hand_written_marshalling_code
  ::std::string_view s1;
  ::std::string_view s2;

 public:
  string_join (::std::string_view str1
	       ,::std::string_view str2) : s1(str1),s2(str2)
  {}

  void Marshal (::cmw::SendBuffer& buf,bool=false) const
  {
    ::cmw::marshalling_integer(s1.length()+s2.length()).Marshal(buf);
    buf.Receive(s1.data(),s1.length());  // Use low-level Receive
    buf.Receive(s2.data(),s2.length());
  }
};

}
