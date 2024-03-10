#ifndef CMW_BUFFER_HH
#define CMW_BUFFER_HH
#include"quicklz.h"
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
#else
#include<errno.h>
#include<fcntl.h>//open
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>//chdir
#endif

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

template<class E=Failure>[[noreturn]]void raise (char const* s,auto...t){
  E e{s};
  apps(e,t...);
  throw e;
}

inline int getError (){
#ifdef CMW_WINDOWS
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline int fromChars (::std::string_view s){
  int n=0;
  ::std::from_chars(s.data(),s.data()+s.size(),n);
  return n;
}


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
      ::uint8_t a=b.giveOne();
      val+=(a&127)*shift;
      if((a&128)==0)return;
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
      if(0==n){b.receive(a);break;}
      a|=128;
      b.receive(a);
      --n;
    }
  }
};
inline bool operator== (MarshallingInt l,MarshallingInt r){return l()==r();}
inline bool operator== (MarshallingInt l,::int32_t r){return l()==r;}

inline void exitFailure (){::std::exit(EXIT_FAILURE);}
#ifdef CMW_WINDOWS
using sockType=SOCKET;
#else
using sockType=int;
inline void setDirectory (char const* d){
  if(::chdir(d)==-1)raise("setDirectory",d,errno);
}

inline int Write (int fd,void const* data,int len){
  if(int r=::write(fd,data,len);r>=0)return r;
  raise("Write",errno);
}

inline int Read (int fd,void* data,int len){
  int r=::read(fd,data,len);
  if(r>0)return r;
  if(r==0)raise("Read eof",len);
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  raise("Read",len,errno);
}

class FileWrapper{
  int d=-2;
 public:
  FileWrapper (){}
  FileWrapper (char const* name,int flags,mode_t mode):
        d{::open(name,flags,mode)} {if(d<0)raise("FileWrapper",name,errno);}

  FileWrapper (char const* name,mode_t mode):
        FileWrapper(name,O_CREAT|O_WRONLY|O_TRUNC,mode){}

  FileWrapper (FileWrapper const&)=delete;
  void operator= (FileWrapper&)=delete;
  FileWrapper (FileWrapper&& o)noexcept:d{o.d}{o.d=-2;}

  void operator= (FileWrapper&& o)noexcept{
    d=o.d;
    o.d=-2;
  }
  auto operator() (){return d;}
  ~FileWrapper (){if(d>0)::close(d);}
};

struct FileBuffer{
  FileWrapper fl;
  char buf[4096];
  char line[120];
  int ind=0;
  int bytes=0;

  FileBuffer (char const* name,int flags):fl(name,flags,0){}

  char getc (){
    if(ind>=bytes){
      bytes=Read(fl(),buf,sizeof buf);
      ind=0;
    }
    return buf[ind++];
  }

  auto getline (char delim='\n'){
    ::std::size_t idx=0;
    while((line[idx]=getc())!=delim){
      if(line[idx]=='\r')raise("getline carriage return");
      if(++idx>=sizeof line)raise("getline line too long");
    }
    line[idx]=0;
    return ::std::string_view{line,idx};
  }
};
#endif

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

inline void closeSocket (sockType s){
#ifdef CMW_WINDOWS
  if(::closesocket(s)==SOCKET_ERROR)
#else
  if(::close(s)==-1)
#endif
    raise("closeSocket",getError());
}

inline int sockWrite (sockType s,void const* data,int len
               ,sockaddr const* addr=nullptr,socklen_t toLen=0){
  if(int r=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);r>0)
    return r;
  raise("sockWrite",s,getError());
}

inline int sockRead (sockType s,void* data,int len,sockaddr* addr,socklen_t* fromLen){
  if(int r=::recvfrom(s,static_cast<char*>(data),len,0,addr,fromLen);r>=0)
    return r;
  auto e=getError();
#ifdef CMW_WINDOWS
  if(WSAETIMEDOUT==e)
#else
  if(EAGAIN==e||EWOULDBLOCK==e)
#endif
    return 0;
  raise("sockRead",len,e);
}

struct SameFormat{
  static void read (auto& b,auto& data){b.give(&data,sizeof(data));}

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
  {for(auto c=0;c<sizeof(val);++c)val|=b.giveOne()<<8*c;}
};

struct MostSignificantFirst:MixedEndian{
  static void read (auto& b,auto& val)
  {for(auto c=sizeof(val);c>0;--c)val|=b.giveOne()<<8*(c-1);}
};

template<class R,class Z>class ReceiveBuffer{
  int msgLength=0;
  int subTotal=0;
 protected:
  char* const rbuf;
  int packetLength=0;
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

  char giveOne (){
    checkLen(1);
    return rbuf[subTotal+rindex++];
  }

  template<class T>T give (){
    T t;
    if(sizeof(T)==1)give(&t,1);
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

  bool update (){
    msgLength=subTotal=0;
    return nextMessage();
  }

  void giveBlock (auto* data,unsigned int elements){
    if(sizeof(*data)==1)give(data,elements);
    else R::readBlock(*this,data,elements);
  }

#ifndef CMW_WINDOWS
  void giveFile (auto nm){
    int sz=give<::uint32_t>();
    checkLen(sz);
    FileWrapper fl{nm,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH};
    while(sz>0){
      int r=Write(fl(),rbuf+subTotal+rindex,sz);
      sz-=r;
      rindex+=r;
    }
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
    MarshallingInt len{*this};
    checkLen(len());
    ::std::string_view s(rbuf+subTotal+rindex,len());
    rindex+=len();
    return s;
  }
};

template<class T>T give (auto& b){return b.template give<T>();}

bool giveBool (auto& b){
  switch(b.giveOne()){
    case 0:return false;
    case 1:return true;
    default:raise("giveBool");
  }
}

void giveVec (auto& buf,auto& v){
  ::int32_t n=::cmw::give<::uint32_t>(buf);
  v.resize(v.size()+n);
  buf.giveBlock(&*(v.end()-n),n);
}

template<class Z>class SendBuffer{
  SendBuffer (SendBuffer const&)=delete;
  SendBuffer& operator= (SendBuffer);
  Z savedSize=0;
 protected:
  Z index=0;
  Z const bufsize;
  unsigned char* const buf;
 public:
  sockType sock_=-1;

  SendBuffer (unsigned char* addr,Z sz):bufsize(sz),buf(addr){}

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

  void receive (arithmetic auto t){
    receive(&t,sizeof t);
  }

  auto receiveAt (int where,arithmetic auto t){
    ::std::memcpy(buf+where,&t,sizeof t);
    return t;
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

  bool flush ();

  //UDP-friendly alternative to flush
  void send (::sockaddr* addr=nullptr,::socklen_t len=0)
  {sockWrite(sock_,buf,index,addr,len);}

  int getIndex (){return index;}
  int getSize (){return bufsize;}

#ifndef CMW_WINDOWS
  void receiveFile (char const* n,::int32_t sz){
    receive(sz);
    auto prev=reserveBytes(sz);
    FileWrapper fl{n,O_RDONLY,0};
    if(Read(fl(),buf+prev,sz)!=sz)raise("SendBuffer receiveFile");
  }
#endif

  void receiveMulti (auto*,auto...);
};

template<class Z>
bool SendBuffer<Z>::flush (){
  int const bytes=sockWrite(sock_,buf,index);
  if(bytes==index){
    reset();
    return true;
  }

  index-=bytes;
  savedSize=index;
  ::std::memmove(buf,buf+bytes,index);
  return false;
}

void receiveBool (auto&b,bool bl){b.template receive<unsigned char>(bl);}

void receive (auto& b,::std::string_view s){
  MarshallingInt(s.size()).marshal(b);
  b.receive(s.data(),s.size());
}

using stringPlus=::std::initializer_list<::std::string_view>;
void receive (auto& b,stringPlus lst){
  ::int32_t t=0;
  for(auto s:lst)t+=s.size();
  MarshallingInt{t}.marshal(b);
  for(auto s:lst)b.receive(s.data(),s.size());//Use low-level receive
}

template<template<class...>class C,class T>
void receiveBlock (auto& b,C<T>const& c){
  ::int32_t n=c.size();
  b.receive(n);
  if constexpr(arithmetic<T>)b.receive(c.data(),n*sizeof(T));
  else
    for(auto const& e:c)
      if constexpr(::std::is_pointer_v<T>)e->marshal(b);
      else e.marshal(b);
}

inline constexpr auto udpPacketMax=1500;
template<class R,class Z=::std::int16_t,int N=udpPacketMax>
class BufferStack:public SendBuffer<Z>,public ReceiveBuffer<R,Z>{
  unsigned char ar[N];
 public:
  BufferStack ():SendBuffer<Z>(ar,N),ReceiveBuffer<R,Z>((char*)ar){}
  explicit BufferStack (int s):BufferStack(){this->sock_=s;}

  bool getPacket (::sockaddr* addr=nullptr,::socklen_t* len=nullptr){
    this->packetLength=sockRead(this->sock_,ar,N,addr,len);
    return this->update();
  }
};

#ifndef CMW_WINDOWS
auto myMin (auto a,auto b){return a<b?a:b;}
constexpr auto qlzFormula (int i){return i+(i>>3)+400;}

template<class R,class Z,int sz>class BufferCompressed:public SendBuffer<Z>,public ReceiveBuffer<R,Z>{
  ::qlz_state_compress comp{};
  ::qlz_state_decompress decomp{};
  unsigned char sendBuf[sz];
  char compBuf[qlzFormula(sz)];
  char recBuf[sz];
  char* compressedStart;
  int compPacketSize;
  int compIndex=0;
  int bytesRead=0;
  bool kosher=true;

 public:
  BufferCompressed ():SendBuffer<Z>(sendBuf,sz),ReceiveBuffer<R,Z>(recBuf){}

  void compress (){
    if(qlzFormula(this->index)>(qlzFormula(sz)-compIndex))
      raise("Not enough room in compressed buf");
    compIndex+=::qlz_compress(this->buf,compBuf+compIndex,this->index,&comp);
    this->reset();
  }

  bool all (int bytes){
    compIndex-=bytes;
    if(0==compIndex)return true;
    ::std::memmove(compBuf,compBuf+bytes,compIndex);
    return false;
  }

  auto data (){return compBuf;}
  auto size (){return compIndex;}

  auto getDuo (){
    return bytesRead<9?::std::span<char>(rbuf+bytesRead,9-bytesRead):
                       ::std::span<char>(compressedStart+bytesRead,compPacketSize-bytesRead);
  }

  using ReceiveBuffer<R,Z>::rbuf;
  bool gotIt (int rc){
    if((bytesRead+=rc)<9)return false;
    if(bytesRead==9){
      if((compPacketSize=::qlz_size_compressed(rbuf))>this->bufsize||
         (this->packetLength=::qlz_size_decompressed(rbuf))>this->bufsize){
        kosher=false;
        raise("gotIt too big",compPacketSize,this->packetLength,this->bufsize);
      }
      compressedStart=rbuf+this->bufsize-compPacketSize;
      ::std::memmove(compressedStart,rbuf,9);
      return false;
    }

    if(bytesRead<compPacketSize)return false;
    ::qlz_decompress(compressedStart,rbuf,&decomp);
    bytesRead=0;
    this->update();
    return true;
  }

  bool gotPacket (){
    if(kosher){
      auto sp=getDuo();
      return gotIt(Read(this->sock_,sp.data(),sp.size()));
    }
    bytesRead+=Read(this->sock_,rbuf,myMin(this->bufsize,compPacketSize-bytesRead));
    if(bytesRead==compPacketSize){
      kosher=true;
      bytesRead=0;
    }
    return false;
  }

  void compressedReset (){
    this->reset();
    comp={};
    decomp={};
    compIndex=bytesRead=0;
    kosher=true;
    closeSocket(this->sock_);
  }
};
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
    len=s.size();
    if(len()>=N)raise("FixedString operator=");
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

inline int preserveError (auto s){
  auto e=getError();
  closeSocket(s);
  return e;
}

struct SockaddrWrapper{
  ::sockaddr_in sa;
  SockaddrWrapper (char const* node,::uint16_t port):sa{AF_INET,::htons(port),{0},{0}}{
    if(int rc=::inet_pton(AF_INET,node,&sa.sin_addr);rc!=1)
      raise("inet_pton",rc);
  }
};

inline auto udpServer (char const* port){
  auto s=::socket(AF_INET,SOCK_DGRAM,0);
  SockaddrWrapper sa("127.0.0.1",fromChars(port));
  if(0==::bind(s,(sockaddr*)&sa,sizeof(sa)))return s;
  raise("udpServer",preserveError(s));
}

inline void winStart (){
#ifdef CMW_WINDOWS
  WSADATA w;
  if(auto r=::WSAStartup(MAKEWORD(2,2),&w);0!=r)raise("WSAStartup",r);
#endif
}
}
#endif
