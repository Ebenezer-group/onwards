#pragma once
#include"Buffer.hh"
#include"ErrorWords.hh"
#include"marshallingInt.hh"
#include<string_view>

#include<fcntl.h>//open
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>//close

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

template<class T>
class emptyContainer{
public:
  template<class R>
  explicit emptyContainer (ReceiveBuffer<R>& b){
    for(auto n=marshallingInt{b}();n>0;--n)T{b};
  }
};
}
