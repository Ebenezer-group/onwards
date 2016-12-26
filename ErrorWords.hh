#pragma once

#include <exception>
#include <string>
#include <experimental/string_view>
#include <utility> // move

namespace cmw {
class failure : public ::std::exception {
  ::std::string whatStr;

public:
  explicit failure (char const* w):whatStr(w) {}
  explicit failure (::std::string& w):whatStr(::std::move(w)) {}

  explicit failure (char const* w, int tot) {
    if(tot>0)whatStr.reserve(tot);
    whatStr=w;
  }

  char const* what () const throw() { return whatStr.c_str(); }

  failure& operator<< (::std::string const& s)
  {
    whatStr.append(s);
    return *this;
  }

  failure& operator<< (::std::experimental::string_view const& s)
  {
    whatStr.append(s.data(),s.size());
    return *this;
  }

  failure& operator<< (char* s)
  {
    whatStr.append(s);
    return *this;
  }

  failure& operator<< (char const* s);
  failure& operator<< (int i);

  template <class T>
  failure& operator<< (T val)
  {
    using ::std::to_string;
    return *this<<to_string(val);
  }
};

class connection_lost : public failure {
public:
  explicit connection_lost (char const* w):failure(w) {}

  template <class T>
  connection_lost& operator<< (T val)
  {
    failure::operator<<(val);
    return *this;
  }
};

}
