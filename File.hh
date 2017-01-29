#pragma once

#include "platforms.hh"

#include "ErrorWords.hh"
#include <string>
#include <string_view>
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
  explicit File (char const *n):name(n){}
  explicit File (::std::string_view n):name(n){}
  File (File&& mv) noexcept :name(::std::move(mv.name)){} // No need to copy fd.

  template <class R>
  explicit File (ReceiveBuffer<R>& buf):name(buf.GiveString_view())
  {
#if defined(CMW_WINDOWS)
    fd=CreateFile(name.c_str(),GENERIC_WRITE,FILE_SHARE_READ
                  ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
#else
    fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_TRUNC
              ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#endif
    if(fd<0)throw failure("File::File open ")<<name<<" "<<GetError();
    buf.GiveFile(fd);
  }

  template <class R>
  File (bool contents,ReceiveBuffer<R>& buf):
        File{contents?File{buf}:File{buf.GiveString_view()}}{}

  ~File ();
  bool Marshal (SendBuffer& buf,bool=false) const;

  ::std::string const& Name () const {return name;}
};

}
