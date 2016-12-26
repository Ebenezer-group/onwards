#pragma once

#include "platforms.hh"

#include "ErrorWords.hh"
#include <string>
#include <utility> // move
#if !defined(CMW_WINDOWS)
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

namespace cmw {
class SendBuffer;
template <class R> class ReceiveBuffer;

class File
{
  ::std::string name;
  file_type mutable fd=0;

public:
  explicit File () {}
  explicit File (char const* n):name(n) {}

  template <class R>
  explicit File (ReceiveBuffer<R>& buf):name(buf.GiveString())
  {
#if defined(CMW_WINDOWS)
    fd=CreateFile(name.c_str(),GENERIC_WRITE,FILE_SHARE_READ
                  ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
#else
    fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_TRUNC
              ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#endif
    if(fd<0)
      throw failure("File::File open ")<<name<<" "<<GetError();
    buf.GiveFile(fd);
  }

  File& operator= (File&& rhs)
  {
    name=::std::move(rhs.name);
    return *this;
  }

  ~File ();
  bool Marshal (SendBuffer& buf,bool=false) const;

  ::std::string const& Name () const {return name;}
};

}
