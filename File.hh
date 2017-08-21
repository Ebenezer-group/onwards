#pragma once
#include"ErrorWords.hh"
#include"marshalling_integer.hh"
#include"ReceiveBuffer.hh"
#include"SendBuffer.hh"
#include<string_view>

#include<fcntl.h> //open
#include<string.h> //strrchr
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h> //close

extern int32_t previous_updatedtime;
extern int32_t current_updatedtime;

namespace cmw{
class File{
  ::std::string_view name;

public:
  explicit File (::std::string_view n):name(n){}

  template<class R>
  explicit File (ReceiveBuffer<R>& buf):name(buf.GiveString_view_plus()){
    int d=::open(name.data(),O_WRONLY|O_CREAT|O_TRUNC
                 ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(d<0)throw failure("File open ")<<name.data()<<" "<<errno;
    try{buf.GiveFile(d);}catch(...){::close(d);throw;}
    ::close(d);
  }

  char const* Name ()const{return name.data();}
};


inline bool MarshalFile (char const* fname,SendBuffer& buf){
  struct stat sb;
  if(::stat(fname,&sb)<0)throw failure("MarshalFile stat ")<<fname;
  if(sb.st_mtime>previous_updatedtime){
    if('.'==fname[0]||fname[0]=='/')buf.Receive(::strrchr(fname,'/')+1);
    else buf.Receive(fname);
    buf.InsertNull();

    int d=::open(fname,O_RDONLY);
    if(d<0)throw failure("MarshalFile open ")<<fname<<" "<<errno;
    try{buf.ReceiveFile(d,sb.st_size);}catch(...){::close(d);throw;}
    ::close(d);
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
    for(auto n=marshalling_integer{buf}();n>0;--n)T{buf};
  }
};
}
