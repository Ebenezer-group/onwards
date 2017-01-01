#include "SendBuffer.hh"
#include "marshalling_integer.hh"

void cmw::SendBuffer::Receive (::std::string const& s)
{
  marshalling_integer slen(s.size());
  slen.Marshal(*this);
  Receive(s.data(),slen());
}

void cmw::SendBuffer::Receive (::std::experimental::string_view const& s)
{
  marshalling_integer slen(s.size());
  slen.Marshal(*this);
  Receive(s.data(),slen());
}

