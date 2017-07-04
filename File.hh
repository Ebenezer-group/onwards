#pragma once
#include"ErrorWords.hh"
#include"ReceiveBuffer.hh"
#include"SendBuffer.hh"
#include<string>
#include<string_view>
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

  template<class R>
  explicit File (ReceiveBuffer<R>& buf):name(buf.GiveString_view()){
    fd=::open(name.c_str(),O_WRONLY|O_CREAT|O_TRUNC
              ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(fd<0)throw failure("File::File open ")<<name<<" "<<errno;
    buf.GiveFile(fd);
  }

  ~File (){if(0!=fd)::close(fd);}

  ::std::string const& Name ()const{return name;}
};

bool MarshalFile (char const* name,SendBuffer& buf){
  struct stat sb;
  if(::stat(name,&sb)<0)throw failure("MarshalFile stat ")<<name;
  if(sb.st_mtime>previous_updatedtime){
    if('.'==name[0]||name[0]=='/')buf.Receive(::strrchr(name,'/')+1);
    else buf.Receive(name);

    int fd;
    if((fd=::open(name,O_RDONLY))<0)
      throw failure("MarshalFile open ")<<name<<" "<<errno;
    try{buf.ReceiveFile(fd,sb.st_size);}catch(...){::close(fd);}
    ::close(fd);
    if(sb.st_mtime>current_updatedtime)current_updatedtime=sb.st_mtime;
    return true;
  }
  return false;
}

template<class T>
class empty_container{
public:
  template<class R>
  explicit empty_container (ReceiveBuffer<R>& buf){
    for(auto n=marshalling_integer{buf}.operator()();n>0;--n)T{buf};
  }
};
}
