#pragma once
#include"ErrorWords.hh"
#include"ReceiveBuffer.hh"
#include"SendBuffer.hh"
#include<string>
#include<string_view>
#include<utility> //move
#include<stdint.h>

#include<fcntl.h> //open
#include<string.h> //strrchr
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h> //close

extern int32_t previous_updatedtime;
extern int32_t current_updatedtime;

namespace cmw{
class File{
  ::std::string name;
  int mutable fd=0;

public:
  explicit File (char const *n):name(n){}
  explicit File (::std::string_view n):name(n){}
  File (File&& mv) noexcept :name(::std::move(mv.name)){} // No need to copy fd.

  template<class R>
  explicit File (ReceiveBuffer<R>& buf):name(buf.GiveString_view()){
    fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_TRUNC
              ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(fd<0)throw failure("File::File open ")<<name<<" "<<errno;
    buf.GiveFile(fd);
  }

  template<class R>
  File (bool contents,ReceiveBuffer<R>& buf):
        File{contents?File{buf}:File{buf.GiveString_view()}}{}

  ~File (){if(0!=fd)::close(fd);}

  bool Marshal (SendBuffer& buf,bool=false)const{
    struct stat sb;
    if(::stat(name.c_str(),&sb)<0)throw failure("File::Marshal stat ")<<name;
    if(sb.st_mtime>previous_updatedtime){
      if((fd=::open(name.c_str(),O_RDONLY))<0)
        throw failure("File::Marshal open ")<<name<<" "<<errno;
      if('.'==name[0]||name[0]=='/')buf.Receive(::strrchr(name.c_str(),'/')+1);
      else buf.Receive(name);

      buf.ReceiveFile(fd,sb.st_size);
      if(sb.st_mtime>current_updatedtime)current_updatedtime=sb.st_mtime;
      return true;
    }
    return false;
  }

  ::std::string const& Name ()const{return name;}
};

template<class T>
class empty_container{
public:
  template<class R>
  explicit empty_container (ReceiveBuffer<R>& buf){
    for(auto n=marshalling_integer{buf}.operator()();n>0;--n)T{buf};
  }
};
}
