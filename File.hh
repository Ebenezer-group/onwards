#pragma once

#include "ErrorWords.hh"
#include <string>
#include <string_view>
#include <utility> // move

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> // close

namespace cmw {
class SendBuffer;
template <class R> class ReceiveBuffer;

class File
{
  ::std::string name;
  int mutable fd=0;

public:
  explicit File (char const *n):name(n){}
  explicit File (::std::string_view n):name(n){}
  File (File&& mv) noexcept :name(::std::move(mv.name)){} // No need to copy fd.

  template <class R>
  explicit File (ReceiveBuffer<R>& buf):name(buf.GiveString_view())
  {
    fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_TRUNC
              ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(fd<0)throw failure("File::File open ")<<name<<" "<<errno;
    buf.GiveFile(fd);
  }

  template <class R>
  File (bool contents,ReceiveBuffer<R>& buf):
        File{contents?File{buf}:File{buf.GiveString_view()}}{}

  ~File () {if(0!=fd)::close(fd);}

  bool Marshal (SendBuffer& buf,bool=false) const;

  ::std::string const& Name () const {return name;}
};

}
