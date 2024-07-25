#pragma once
#include<cmwBuffer.hh>
#include<complex>
#if __has_include(<netdb.h>)
#include<netdb.h>
#endif

namespace cmw{
void complexMarshal (auto& buf,auto const& c){
  buf.receive(c.real());
  buf.receive(c.imag());
}

template<class T>
auto complexGive (auto& buf){
  //The following has an order of evaluation problem:
  //::std::complex<T>(give<T>(buf),give<T>(buf));
  T rl=give<T>(buf);
  return ::std::complex<T>(rl,give<T>(buf));
}

template<class T,class...Ts>
::uint8_t marshalSegments (auto const& c,auto& buf){
  ::uint8_t segs=0;
  if(c.template is_registered<T>()){
    if(::int32_t n=c.template size<T>();n>0){
      segs=1;
      buf.receive(T::typeNum);
      buf.receive(n);
      for(T const& t:c.template segment<T>()){t.marshal(buf);}
    }
  }
  if constexpr(sizeof...(Ts)>0)return segs+marshalSegments<Ts...>(c,buf);
  return segs;
}

template<class...Ts>void marshalCollection (auto const& c,auto& buf){
  auto ind=buf.reserveBytes(1);
  buf.receiveAt(ind,marshalSegments<Ts...>(c,buf));
}

template<class T>
void buildSegment (auto& c,auto& buf){
  ::int32_t n=give<::uint32_t>(buf);
  c.template reserve<T>(n);
  for(;n>0;--n){c.template emplace<T>(buf);}
}

void buildCollection (auto& c,auto& buf,auto f){
  for(auto segs=give<::uint8_t>(buf);segs>0;--segs){f(c,buf);}
}

class GetaddrinfoWrapper{
  ::addrinfo *head,*addr;
 public:
  GetaddrinfoWrapper (char const* node,char const* port
                      ,int type,int flags=0){
    ::addrinfo const hints{flags,AF_UNSPEC,type,0,0,0,0,0};
    if(int r=::getaddrinfo(node,port,&hints,&head);r!=0)
      raise("getaddrinfo",::gai_strerror(r));
    addr=head;
  }

  ~GetaddrinfoWrapper (){::freeaddrinfo(head);}
  auto const& operator() ()const{return *addr;}

  sockType getSock (){
    for(;addr!=nullptr;addr=addr->ai_next){
      if(auto s=::socket(addr->ai_family,addr->ai_socktype,0);-1!=s)return s;
    }
    raise("getaddrinfo getSock");
  }

  GetaddrinfoWrapper (GetaddrinfoWrapper&)=delete;
  GetaddrinfoWrapper& operator= (GetaddrinfoWrapper)=delete;
};

inline auto tcpServer (char const* port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_STREAM,AI_PASSIVE};
  auto s=ai.getSock();

  if(int on=1;setsockWrapper(s,SO_REUSEADDR,on)==0
    &&::bind(s,ai().ai_addr,ai().ai_addrlen)==0
    &&::listen(s,SOMAXCONN)==0)return s;
  raise("tcpServer",preserveError(s));
}

#ifdef CMW_VALARRAY_MARSHALLING
#include<valarray>
void valarrayMarshal (auto& buf,auto const& varray){
  int32_t count=varray.size();
  buf.receive(count);
  buf->receive(&varray[0],count*sizeof(T));
}

void valarrayGive (auto& buf,auto& varray){
  int32_t count=give<uint32_t>(buf);
  varray.resize(count);
  buf.give(&varray,count*sizeof(T));
}
#endif
}
