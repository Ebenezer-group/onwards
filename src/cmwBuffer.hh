#pragma once
#include<charconv>
#include<cstdint>
#include<cstdlib>//exit
#include<cstring>//memcpy,memmove
#include<exception>
#include<initializer_list>
#include<limits>
#include<span>
#include<string>
#include<string_view>
#include<type_traits>
#include<utility>
static_assert(::std::numeric_limits<float>::is_iec559,"IEEE754");

#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#include<ws2tcpip.h>
#define CMW_WINDOWS
using sockType=SOCKET;
#else
#include"quicklz.h"
#include<errno.h>
#include<fcntl.h>//open
#include<arpa/inet.h>
#include<sys/mman.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>//chdir
using sockType=int;
#endif

static_assert(sizeof(bool)==1);
template<class T>concept arithmetic=::std::is_arithmetic_v<T>||::std::is_enum_v<T>;

namespace cmw{
class Failure:public ::std::exception{
  ::std::string st;
 public:
  explicit Failure (auto s):st(s){}
  void operator<< (::std::string_view v){(st+=" ")+=v;}

  void operator<< (::int64_t i){
    char b[32];
    auto [ptr,ec]=::std::to_chars(b,b+sizeof b,i);
    *this<<::std::string_view(b,ptr);
  }

  char const* what ()const noexcept{return st.data();}
};

void apps (auto&){}
void apps (auto& e,auto t,auto...ts){
  e<<t;
  apps(e,ts...);
}

[[noreturn]]void raise (char const* s,auto...t){
  Failure e{s};
  apps(e,t...);
  throw e;
}

inline int fromChars (::std::string_view s){
  int n=0;
  ::std::from_chars(s.data(),s.data()+s.size(),n);
  return n;
}

template<class T>T give (auto& b){return b.template give<T>();}

template<class,class>class ReceiveBuffer;
class MarshallingInt{
  ::int32_t val=0;
 public:
  MarshallingInt ()=default;
  explicit MarshallingInt (::int32_t v):val{v}{}
  explicit MarshallingInt (::std::string_view v):val{fromChars(v)}{}

  //Reads a sequence of bytes in variable-length format and
  //builds a 32 bit integer.
  template<class R,class Z>explicit MarshallingInt (ReceiveBuffer<R,Z>& b){
    ::uint32_t shift=1;
    for(;;){
      auto a=give<::uint8_t>(b);
      val+=(a&127)*shift;
      if(!(a&128))return;
      shift<<=7;
      val+=shift;
    }
  }

  void operator= (::int32_t r){val=r;}
  void operator+= (::int32_t r){val+=r;}
  auto operator() ()const{return val;}
  //Encode integer into variable-length format.
  void marshal (auto& b)const{
    ::uint32_t n=val;
    for(;;){
      ::uint8_t a=n&127;
      n>>=7;
      if(!n){receive(b,a);break;}
      a|=128;
      receive(b,a);
      --n;
    }
  }
};
inline bool operator== (MarshallingInt l,MarshallingInt r){return l()==r();}
inline bool operator== (MarshallingInt l,::int32_t r){return l()==r;}

inline void exitFailure (){::std::exit(EXIT_FAILURE);}

inline int getError (){
#ifdef CMW_WINDOWS
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline void closeSocket (sockType s){
#ifdef CMW_WINDOWS
  ::closesocket(s);
#else
  ::close(s);
#endif
}

inline int preserveError (int s){
  auto e=getError();
  closeSocket(s);
  return e;
}

#ifndef CMW_WINDOWS
inline void Write (int fd,char const* data,int len,bool doClose=false){
  for(;;){
    if(int r=::write(fd,data,len);r>0){
      if(!(len-=r))return;
      data+=r;
    }else{
      if(errno!=EINTR)raise("Write",doClose?preserveError(fd):errno);
    }
  }
}

inline int Read (int fd,void* data,int len,bool doClose=false){
  if(int r=::read(fd,data,len);r>0)return r;
  raise("Read",len,doClose?preserveError(fd):errno);
}

inline int Open (auto nm,int flags,mode_t md=0){
  if(int d=::open(nm,flags,md);d>0)return d;
  raise("Open",nm,errno);
}
#endif

struct SameFormat{
  static void read (auto& b,auto& data){b.give(&data,sizeof data);}

  static void readBlock (auto& b,auto data,int elements)
  {b.give(data,elements*sizeof(*data));}
};

struct MixedEndian{
  static void read (auto& b,float& f){
    ::uint32_t tmp;
    read(b,tmp);
    ::std::memcpy(&f,&tmp,sizeof f);
  }

  static void read (auto& b,double& d){
    ::uint64_t tmp;
    read(b,tmp);
    ::std::memcpy(&d,&tmp,sizeof d);
  }

  static void readBlock (auto& b,auto data,int elements){
    for(;elements>0;--elements){read(b,*data++);}
  }
};

struct LeastSignificantFirst:MixedEndian{
  static void read (auto& b,auto& val)
  {for(auto c=0;c<sizeof val;++c)val|=give<::uint8_t>(b)<<8*c;}
};

struct MostSignificantFirst:MixedEndian{
  static void read (auto& b,auto& val)
  {for(auto c=sizeof val;c>0;--c)val|=give<::uint8_t>(b)<<8*(c-1);}
};

template<class R,class Z>class ReceiveBuffer{
  char* const rbuf;
  int msgLength;
  int subTotal;
  int packetLength;
  int rindex=0;

 public:
  explicit ReceiveBuffer (char* addr):rbuf{addr}{}

  void checkLen (int n){
    if(n>msgLength-rindex)raise("ReceiveBuffer checkLen",n,msgLength,rindex);
  }

  void give (void* address,int len){
    checkLen(len);
    ::std::memcpy(address,rbuf+subTotal+rindex,len);
    rindex+=len;
  }

  template<class T>T give (){
    T t;
    if(sizeof t==1)give(&t,1);
    else R::read(*this,t);
    return t;
  }

  bool nextMessage (){
    subTotal+=msgLength;
    if(subTotal<packetLength){
      rindex=0;
      msgLength=sizeof(Z);
      msgLength=give<::std::make_unsigned_t<Z>>();
      return true;
    }
    return false;
  }

  bool update (size_t len){
    packetLength=len;
    msgLength=subTotal=0;
    return nextMessage();
  }

  explicit ReceiveBuffer (::std::span<char> sp):rbuf{sp.data()}{update(sp.size());}

  void giveBlock (auto* data,unsigned int elements){
    if(sizeof(*data)==1)give(data,elements);
    else R::readBlock(*this,data,elements);
  }

#ifndef CMW_WINDOWS
  int giveFile (auto nm){
    int sz=give<::uint32_t>();
    checkLen(sz);
    int fd=Open(nm,O_CREAT|O_WRONLY|O_TRUNC,0644);
    Write(fd,rbuf+subTotal+rindex,sz,true);
    rindex+=sz;
    return fd;
  }
#endif

  template<class T>requires arithmetic<T>
  auto giveSpan (){
    ::int32_t sz=give<::uint32_t>();
    ::int32_t serLen=sizeof(T)*sz;
    checkLen(serLen);
    ::std::span<T> s(reinterpret_cast<T*>(rbuf+subTotal+rindex),sz);
    rindex+=serLen;
    return s;
  }

  auto giveStringView (){
    auto len=MarshallingInt{*this}();
    checkLen(len);
    ::std::string_view s(rbuf+subTotal+rindex,len);
    rindex+=len;
    return s;
  }
};

bool giveBool (auto& b){
  switch(give<::uint8_t>(b)){
    case 0:return false;
    case 1:return true;
    default:raise("giveBool");
  }
}

void giveVec (auto& buf,auto& v){
  ::int32_t n=give<::uint32_t>(buf);
  v.resize(v.size()+n);
  buf.giveBlock(&*(v.end()-n),n);
}

auto cast (auto* t){return reinterpret_cast<::sockaddr const*>(t);}

template<class Z>class SendBuffer{
  SendBuffer (SendBuffer const&)=delete;
  void operator= (SendBuffer&);
  char* const buf;
  Z const bufsize;
  Z savedSize=0;
 protected:
  Z index=0;
 public:
  sockType sock_=-1;

  SendBuffer (auto addr,Z sz):buf(addr),bufsize(sz){}

  int getZ (){return sizeof(Z);}

  int reserveBytes (Z n=sizeof(Z)){
    if(n>bufsize-index)raise("SendBuffer reserveBytes",n,index);
    auto i=index;
    index+=n;
    return i;
  }

  void receive (void const* data,int size){
    ::std::memcpy(buf+reserveBytes(size),data,size);
  }

  void receiveAt (int where,arithmetic auto t){
    ::std::memcpy(buf+where,&t,sizeof t);
  }

  void receiveAt (int where){
    receiveAt(where,index-(where+4));
  }

  void fillInSize (::int32_t max){
    Z marshalledBytes=index-savedSize;
    if(marshalledBytes>max)raise("fillInSize",max);
    receiveAt(savedSize,marshalledBytes);
    savedSize=index;
  }

  void fillInHdr (::int32_t max,auto id){
    receiveAt(savedSize+getZ(),id);
    fillInSize(max);
  }

  void reset (){savedSize=index=0;}
  void rollback (){index=savedSize;}

  bool flush (){
    int const bytes=Write(sock_,buf,index);
    if(bytes==index){
      reset();
      return true;
    }

    index-=bytes;
    savedSize=index;
    ::std::memmove(buf,buf+bytes,index);
    return false;
  }

  template<class T=int>auto send (T* addr=nullptr,::socklen_t len=0){
    if(int r=::sendto(sock_,buf,index,0,cast(addr),len);r>0)return r;
    raise("buf::send",getError());
  }

#ifndef CMW_WINDOWS
  int receiveFile (char const* nm,::int32_t sz){
    receive(&sz,sizeof sz);
    int fd=Open(nm,O_RDONLY);
    Read(fd,buf+reserveBytes(sz),sz,true);
    return fd;
  }
#endif

  void receiveMulti (auto,auto...);
};

auto asFour (auto t){return static_cast<::int32_t>(t);}
void receive (auto& b,arithmetic auto t){b.receive(&t,sizeof t);}

void receive (auto& b,::std::string_view s){
  MarshallingInt(s.size()).marshal(b);
  b.receive(s.data(),s.size());
}

using stringPlus=::std::initializer_list<::std::string_view>;
void receive (auto& b,stringPlus lst){
  int t=0;
  for(auto s:lst)t+=s.size();
  MarshallingInt{t}.marshal(b);
  for(auto s:lst)b.receive(s.data(),s.size());//Use low-level
}

template<template<class...>class C,class T>
void receiveBlock (auto& b,C<T>const& c){
  ::int32_t n=c.size();
  receive(b,n);
  if constexpr(arithmetic<T>)b.receive(c.data(),n*sizeof(T));
  else
    for(auto const& e:c)
      if constexpr(::std::is_pointer_v<T>)e->marshal(b);
      else e.marshal(b);
}

inline constexpr auto udpPacketMax=1500;
template<class R,class Z=::int16_t,int N=udpPacketMax>
class BufferStack:public SendBuffer<Z>,public ReceiveBuffer<R,Z>{
  char ar[N];
 public:
  BufferStack ():SendBuffer<Z>(ar,N),ReceiveBuffer<R,Z>(ar){}
  explicit BufferStack (int s):BufferStack(){this->sock_=s;}

  auto outDuo (){return ::std::span(ar,this->index);}
  auto getDuo (){return ::std::span(ar,N);}

  bool getPacket (::sockaddr* addr=nullptr,::socklen_t* len=nullptr){
    if(int r=::recvfrom(this->sock_,ar,N,0,addr,len);r>=0)return this->update(r);
    raise("getPacket",getError());
  }
};

#ifndef CMW_WINDOWS
auto myMin (auto a,auto b){return a<b?a:b;}
constexpr auto qlzFormula (int i){return i+(i>>3)+400;}

template<class R,class Z,int sz>class BufferCompressed:public SendBuffer<Z>,public ReceiveBuffer<R,Z>{
  ::qlz_state_compress comp{};
  ::qlz_state_decompress decomp{};
  char sendBuf[sz];
  char compBuf[qlzFormula(sz)];
  char recBuf[sz];
  char* compressedStart;
  int compPacketSize;
  int compIndex=0;
  int bytesSent=0;
  int bytesRead=0;
  bool kosher=true;

 public:
  BufferCompressed ():SendBuffer<Z>(sendBuf,sz),ReceiveBuffer<R,Z>(recBuf){}

  void compress (){
    if(qlzFormula(this->index)>(qlzFormula(sz)-compIndex)){
      this->reset();
      raise("compress zone");
    }
    compIndex+=::qlz_compress(sendBuf,compBuf+compIndex,this->index,&comp);
    this->reset();
  }

  void adjustFrame (int bytes){
    bytesSent=0;
    compIndex-=bytes;
    if(compIndex<0)compIndex=0;
    if(compIndex==0)return;
    ::std::memmove(compBuf,compBuf+bytes,compIndex);
  }

  auto outDuo (){
    auto sp=::std::span<char>(compBuf+bytesSent,compIndex-bytesSent);
    bytesSent=compIndex;
    return sp;
  }

  auto getDuo (){
    return bytesRead<9?::std::span(recBuf+bytesRead,9-bytesRead):
                       ::std::span(compressedStart+bytesRead,compPacketSize-bytesRead);
  }

  bool gotIt (int rc){
    if((bytesRead+=rc)<9)return false;
    if(bytesRead==9){
      if((compPacketSize=::qlz_size_compressed(recBuf))>sz||
         ::qlz_size_decompressed(recBuf)>sz){
        kosher=false;
        raise("gotIt size",compPacketSize,sz);
      }
      compressedStart=recBuf+sz-compPacketSize;
      ::std::memmove(compressedStart,recBuf,9);
      return false;
    }

    if(bytesRead<compPacketSize)return false;
    bytesRead=0;
    this->update(::qlz_decompress(compressedStart,recBuf,&decomp));
    return true;
  }

  bool gotPacket (){
    if(kosher){
      auto sp=getDuo();
      return gotIt(Read(this->sock_,sp.data(),sp.size()));
    }
    bytesRead+=Read(this->sock_,recBuf,myMin(sz,compPacketSize-bytesRead));
    if(bytesRead==compPacketSize){
      kosher=true;
      bytesRead=0;
    }
    return false;
  }

  void flush (){
    Write(this->sock_,compBuf,compIndex);
    compIndex=0;
  }

  void compressedReset (){
    this->reset();
    comp={};
    decomp={};
    compIndex=bytesSent=bytesRead=0;
    kosher=true;
  }
};

inline void setDirectory (char const* d){
  if(::chdir(d)!=0)raise("setDirectory",d,errno);
}

template<class T=void*>
auto mmapWrapper (size_t len){
  if(auto addr=::mmap(0,len,PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS,-1,0);addr!=MAP_FAILED)
    return reinterpret_cast<T>(addr);
  raise("mmap",errno);
}
#endif

template<int N>class FixedString{
  char str[N];
  MarshallingInt len{};

 public:
  FixedString ()=default;

  explicit FixedString (::std::string_view s):len(s.size()){
    if(len()>=N)raise("FixedString ctor");
    ::std::memcpy(str,s.data(),len());
    str[len()]=0;
  }

  template<class R,class Z>
  explicit FixedString (ReceiveBuffer<R,Z>& b):len(b){
    if(len()>=N)raise("FixedString stream ctor");
    b.give(str,len());
    str[len()]=0;
  }

  FixedString (FixedString const& o):len(o.len()){
    ::std::memcpy(str,o.str,len());
  }

  void operator= (::std::string_view s){
    if(s.size()>=N)raise("FixedString operator=");
    len=s.size();
    ::std::memcpy(str,s.data(),len());
  }

  void marshal (auto& b)const{
    len.marshal(b);
    b.receive(str,len());
  }

  unsigned int bytesAvailable ()const{return N-(len()+1);}

  auto append (::std::string_view s){
    if(bytesAvailable()>=s.size()){
      ::std::memcpy(str+len(),s.data(),s.size());
      len+=s.size();
      str[len()]=0;
    }
    return str;
  }

  char* operator() (){return str;}
  char const* data ()const{return str;}
  char operator[] (int i)const{return str[i];}
};
using FixedString60=FixedString<60>;
using FixedString120=FixedString<120>;

class SockaddrWrapper{
  ::sockaddr_in sa;
 public:
  SockaddrWrapper (::uint16_t port):sa{AF_INET,::htons(port),{},{}}{}
  SockaddrWrapper (char const* node,::uint16_t port):SockaddrWrapper(port){
    if(int rc=::inet_pton(AF_INET,node,&sa.sin_addr);rc!=1)
      raise("inet_pton",rc);
  }
  auto operator() ()const{return cast(&sa);}
};

inline int udpServer (::uint16_t port){
  int s=::socket(AF_INET,SOCK_DGRAM,0);
  SockaddrWrapper sa{port};
  if(0==::bind(s,sa(),sizeof sa))return s;
  raise("udpServer",preserveError(s));
}

auto setsockWrapper (sockType s,int opt,auto t){
  return ::setsockopt(s,SOL_SOCKET,opt,reinterpret_cast<char*>(&t),sizeof t);
}

inline void setRcvTimeout (sockType s,int time){
#ifdef CMW_WINDOWS
  DWORD t=time*1000;
#else
  ::timeval t{time,0};
#endif
  if(setsockWrapper(s,SO_RCVTIMEO,t)!=0)raise("setRcvTimeout",getError());
}

inline void winStart (){
#ifdef CMW_WINDOWS
  WSADATA w;
  if(auto r=::WSAStartup(MAKEWORD(2,2),&w);r)raise("WSAStartup",r);
#endif
}
}
