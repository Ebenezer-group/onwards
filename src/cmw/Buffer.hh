#ifndef CMW_Buffer_hh
#define CMW_Buffer_hh 1
#include"quicklz.h"
#include<algorithm>//min
#include<array>
#include<charconv>//from_chars
#include<exception>
#include<initializer_list>
#include<limits>
#include<string>
#include<string_view>
#include<type_traits>
static_assert(::std::numeric_limits<unsigned char>::digits==8,"");
static_assert(::std::numeric_limits<float>::is_iec559,"Only IEEE754 supported");

#include<stdint.h>
#include<stdio.h>//snprintf
#include<stdlib.h>//strtol,exit
#include<string.h>//memcpy,memmove,strlen
#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#include<winsock2.h>
#include<ws2tcpip.h>
#define CMW_WINDOWS
#define poll WSAPoll
using sockType=SOCKET;
using fileType=HANDLE;
#else
#include<errno.h>
#include<fcntl.h>//fcntl,open
#include<netdb.h>
#include<poll.h>
#include<sys/socket.h>
#include<sys/stat.h>//open
#include<sys/types.h>
#include<syslog.h>
#include<unistd.h>//close,chdir,read,write
#define _MSVC_LANG 0
using sockType=int;
using fileType=int;
#endif

namespace cmw{
inline int getError (){
#ifdef CMW_WINDOWS
  return WSAGetLastError();
#else
  return errno;
#endif
}

class failure:public ::std::exception{
  ::std::string str;
public:
  explicit failure (char const* s):str(s){}
  explicit failure (::std::string_view s):str(s){}
  void operator<< (::std::string_view const& s){str.append(" "); str.append(s);}
  void operator<< (char const* s){str.append(" "); str.append(s);}
  void operator<< (int i){char b[12]; ::snprintf(b,sizeof b,"%d",i);*this<<b;}
  char const* what ()const noexcept{return str.c_str();}
};
struct fiasco:failure{explicit fiasco(char const* s):failure(s){}};

template<class E>void apps (E& e){throw e;}
template<class E,class T,class...Ts>void apps (E& e,T t,Ts...ts){
  e<<t; apps(e,ts...);
}

template<class...T>void raise (char const* s,T...t){
  failure f(s); apps(f,t...);
}
template<class...T>void raiseFiasco (char const* s,T...t){
  fiasco f(s); apps(f,t...);
}

inline void winStart (){
#ifdef CMW_WINDOWS
  WSADATA w;
  if(auto r=::WSAStartup(MAKEWORD(2,2),&w);0!=r)raise("WSAStartup",r);
#endif
}

inline int fromChars (char const* p){
  int res=0;
  ::std::from_chars(p,p+::strlen(p),res);
  return res;
}

struct FILE_wrapper{
  ::FILE* hndl;
  char line[120];

  FILE_wrapper (char const* fn,char const* mode){
    if((hndl=::fopen(fn,mode))==nullptr)raise("FILE_wrapper",fn,mode,errno);
  }
  char* fgets (){return ::fgets(line,sizeof line,hndl);}
  ~FILE_wrapper (){::fclose(hndl);}
};

class getaddrinfoWrapper{
  ::addrinfo* head,*addr;
 public:
  getaddrinfoWrapper (char const* node,char const* port,int type,int flags=0){
    ::addrinfo hints{flags,AF_UNSPEC,type,0,0,0,0,0};
    int rc=::getaddrinfo(node,port,&hints,&head);
    if(rc!=0)raise("getaddrinfo",::gai_strerror(rc));
    addr=head;
  }

  ~getaddrinfoWrapper (){::freeaddrinfo(head);}
  ::addrinfo* operator() (){return addr;}
  void inc (){if(addr!=nullptr)addr=addr->ai_next;}
  sockType getSock (){
    for(;addr!=nullptr;addr=addr->ai_next){
      if(auto s=::socket(addr->ai_family,addr->ai_socktype,0);-1!=s)return s;
    }
    raise("getaddrinfo getSock");
  }
  getaddrinfoWrapper (getaddrinfoWrapper const&)=delete;
  getaddrinfoWrapper& operator= (getaddrinfoWrapper)=delete;
};

inline void setDirectory (char const* d){
#ifdef CMW_WINDOWS
  if(!::SetCurrentDirectory(d))
#else
  if(::chdir(d)==-1)
#endif
    raise("setDirectory",d,getError());
}

inline int pollWrapper (::pollfd* fds,int n,int timeout=-1){
  if(int r=::poll(fds,n,timeout);r>=0)return r;
  raise("poll",getError());
}

template<class T>int setsockWrapper (sockType s,int opt,T t){
  return ::setsockopt(s,SOL_SOCKET,opt,reinterpret_cast<char*>(&t),sizeof t);
}

inline void setRcvTimeout (sockType s,int time){
#ifdef CMW_WINDOWS
  DWORD t=time*1000;
#else
  timeval t{time,0};
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

inline int preserveError (sockType s){
  auto e=getError();
  closeSocket(s);
  return e;
}

inline sockType connectWrapper (char const* node,char const* port){
  getaddrinfoWrapper ai(node,port,SOCK_STREAM);
  auto s=ai.getSock();
  if(0==::connect(s,ai()->ai_addr,ai()->ai_addrlen))return s;
  errno=preserveError(s);
  return -1;
}

inline int setNonblocking (sockType s){
#ifndef CMW_WINDOWS
  return ::fcntl(s,F_SETFL,O_NONBLOCK);
#endif
}

inline sockType udpServer (char const* port){
  getaddrinfoWrapper ai(nullptr,port,SOCK_DGRAM,AI_PASSIVE);
  auto s=ai.getSock();
  if(0==::bind(s,ai()->ai_addr,ai()->ai_addrlen))return s;
  raise("udpServer",preserveError(s));
}

inline sockType tcpServer (char const* port){
  getaddrinfoWrapper ai(nullptr,port,SOCK_STREAM,AI_PASSIVE);
  auto s=ai.getSock();

  if(int on=1;setsockWrapper(s,SO_REUSEADDR,on)==0
    &&::bind(s,ai()->ai_addr,ai()->ai_addrlen)==0
    &&::listen(s,SOMAXCONN)==0)return s;
  raise("tcpServer",preserveError(s));
}

inline int acceptWrapper(sockType s){
  if(int n=::accept(s,nullptr,nullptr);n>=0)return n;
  auto e=getError();
  if(ECONNABORTED==e)return 0;
  raise("acceptWrapper",e);
}

inline int sockWrite (sockType s,void const* data,int len
                      ,sockaddr const* addr=nullptr,socklen_t toLen=0){
  int r=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);
  if(r>0)return r;
  auto e=getError();
  if(EAGAIN==e||EWOULDBLOCK==e)return 0;
  raise("sockWrite",s,e);
}

inline int sockRead (sockType s,void* data,int len
                     ,sockaddr* addr=nullptr,socklen_t* fromLen=nullptr){
  int r=::recvfrom(s,static_cast<char*>(data),len,0,addr,fromLen);
  if(r>0)return r;
  auto e=getError();
  if(0==r||ECONNRESET==e)raiseFiasco("sockRead eof",s,len,e);
  if(EAGAIN==e||EWOULDBLOCK==e
#ifdef CMW_WINDOWS
     ||WSAETIMEDOUT==e
#endif
  )return 0;
  raise("sockRead",s,len,e);
}

class SendBuffer;
template<class R>class ReceiveBuffer;
class MarshallingInt{
  int32_t val;
public:
  MarshallingInt (){}
  explicit MarshallingInt (int32_t v):val(v){}
  explicit MarshallingInt (char const* v):val(fromChars(v)){}

  //Reads a sequence of bytes in variable-length format and
  //builds a 32 bit integer.
  template<class R>
  explicit MarshallingInt (ReceiveBuffer<R>& b):val(0){
    uint32_t shift=1;
    for(;;){
      uint8_t a=b.giveOne();
      val+=(a&127)*shift;
      if((a&128)==0)return;
      shift<<=7;
      val+=shift;
    }
  }

  void operator= (int32_t r){val=r;}
  int32_t operator() ()const{return val;}
  void marshal (SendBuffer&)const;
};
inline bool operator== (MarshallingInt l,MarshallingInt r){return l()==r();}
inline bool operator== (MarshallingInt l,int32_t r){return l()==r;}

#ifdef CMW_WINDOWS
inline DWORD Write (HANDLE h,void const* data,int len){
  DWORD bytesWritten=0;
  if(!WriteFile(h,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    raise("Write",GetLastError());
  return bytesWritten;
}

inline DWORD Read (HANDLE h,void* data,int len){
  DWORD bytesRead=0;
  if(!ReadFile(h,static_cast<char*>(data),len,&bytesRead,nullptr))
    raise("Read",GetLastError());
  return bytesRead;
}
#else
inline int Write (int fd,void const* data,int len){
  if(int r=::write(fd,data,len);r>=0)return r;
  raise("Write",errno);
}

inline int Read (int fd,void* data,int len){
  int r=::read(fd,data,len);
  if(r>0)return r;
  if(r==0)raiseFiasco("Read eof",len);
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  raise("Read",len,errno);
}

template<class...T>void syslogWrapper (int pri,char const* fmt,T... t){
  ::syslog(pri,fmt,t...);
}

template<class...T>void bail (char const* fmt,T... t)noexcept{
  syslogWrapper(LOG_ERR,fmt,t...);
  ::exit(EXIT_FAILURE);
}

struct fileWrapper{
  int const d;
  fileWrapper ():d(-2){}
  fileWrapper (char const* name,int flags,mode_t mode=0):
          d(::open(name,flags,mode)){
    if(d<0)raise("fileWrapper",name,errno);
  }
  ~fileWrapper (){::close(d);}
};

class File{
  ::std::string_view name;
public:
  explicit File (::std::string_view n):name(n){}

  template<class R>
  explicit File (ReceiveBuffer<R>& buf):name(giveStringView_plus(buf)){
    fileWrapper fl(name.data(),O_WRONLY|O_CREAT|O_TRUNC
                   ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    buf.giveFile(fl.d);
  }

  char const* Name ()const{return name.data();}
};
template<class R>void giveFiles (ReceiveBuffer<R>& b){
  for(auto n=MarshallingInt{b}();n>0;--n)File{b};
}
#endif

struct SameFormat{
  template<template<class> class B,class U>
  void Read (B<SameFormat>& buf,U& data){buf.give(&data,sizeof(U));}

  template<template<class> class B,class U>
  void ReadBlock (B<SameFormat>& buf,U* data,int elements)
  {buf.give(data,elements*sizeof(U));}
};

struct LeastSignificantFirst{
  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint8_t& val)
  {val=buf.giveOne();}

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint16_t& val){
    val=buf.giveOne();
    val|=buf.giveOne()<<8;
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint32_t& val){
    val=buf.giveOne();
    val|=buf.giveOne()<<8;
    val|=buf.giveOne()<<16;
    val|=buf.giveOne()<<24;
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint64_t& val){
    val=buf.giveOne();
    val|=buf.giveOne()<<8;
    val|=buf.giveOne()<<16;
    val|=buf.giveOne()<<24;
    val|=(uint64_t)buf.giveOne()<<32;
    val|=(uint64_t)buf.giveOne()<<40;
    val|=(uint64_t)buf.giveOne()<<48;
    val|=(uint64_t)buf.giveOne()<<56;
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,float& f){
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&f,&tmp,sizeof f);
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,double& d){
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&d,&tmp,sizeof d);
  }

  template<template<class> class B,class U>
  void ReadBlock (B<LeastSignificantFirst>& buf,U* data,int elements){
    for(int i=0;i<elements;++i){*(data+i)=buf.template give<U>();}
  }

  //Overloads for uint8_t and int8_t
  template<template<class> class B>
  void ReadBlock (B<LeastSignificantFirst>& buf,uint8_t* data,int elements){
    buf.give(data,elements);
  }

  template<template<class> class B>
  void ReadBlock (B<LeastSignificantFirst>& buf,int8_t* data,int elements){
    buf.give(data,elements);
  }
};

struct MostSignificantFirst{
  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint8_t& val)
  {val=buf.giveOne();}

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint16_t& val){
    val=buf.giveOne()<<8;
    val|=buf.giveOne();
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint32_t& val){
    val=buf.giveOne()<<24;
    val|=buf.giveOne()<<16;
    val|=buf.giveOne()<<8;
    val|=buf.giveOne();
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint64_t& val){
    val=(uint64_t)buf.giveOne()<<56;
    val|=(uint64_t)buf.giveOne()<<48;
    val|=(uint64_t)buf.giveOne()<<40;
    val|=(uint64_t)buf.giveOne()<<32;
    val|=buf.giveOne()<<24;
    val|=buf.giveOne()<<16;
    val|=buf.giveOne()<<8;
    val|=buf.giveOne();
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,float& f){
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&f,&tmp,sizeof f);
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,double& d){
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&d,&tmp,sizeof d);
  }

  template<template<class> class B,class U>
  void ReadBlock (B<MostSignificantFirst>& buf,U* data,int elements){
    for(int i=0;i<elements;++i){*(data+i)=buf.template give<U>();}
  }

  template<template<class> class B>
  void ReadBlock (B<MostSignificantFirst>& buf,uint8_t* data,int elements){
    buf.give(data,elements);
  }

  template<template<class> class B>
  void ReadBlock (B<MostSignificantFirst>& buf,int8_t* data,int elements){
    buf.give(data,elements);
  }
};

template<class R> class ReceiveBuffer{
  R reader;
  int msgLength=0;
  int subTotal=0;
protected:
  int rindex=0;
  int packetLength;
  char* const rbuf;
public:
  ReceiveBuffer (char* addr,int bytes):packetLength(bytes),rbuf(addr){}

  void checkData (int n){
    if(n>msgLength-rindex)raise("ReceiveBuffer checkData",n,msgLength,rindex);
  }

  void give (void* address,int len){
    checkData(len);
    ::memcpy(address,rbuf+subTotal+rindex,len);
    rindex+=len;
  }

  char giveOne (){
    checkData(1);
    return rbuf[subTotal+rindex++];
  }

  template<class T>T give (){
    T tmp;
    reader.Read(*this,tmp);
    return tmp;
  }

  bool NextMessage (){
    subTotal+=msgLength;
    if(subTotal<packetLength){
      rindex=0;
      msgLength=sizeof(int32_t);
      msgLength=give<uint32_t>();
      return true;
    }
    return false;
  }

  bool Update (){
    msgLength=subTotal=0;
    return NextMessage();
  }

  template<class T>void giveBlock (T* data,unsigned int elements)
  {reader.ReadBlock(*this,data,elements);}

  void giveFile (fileType d){
    int sz=give<uint32_t>();
    checkData(sz);
    while(sz>0){
      int rc=Write(d,rbuf+subTotal+rindex,sz);
      sz-=rc;
      rindex+=rc;
    }
  }

  template<class T>T giveStringy (){
    MarshallingInt len(*this);
    checkData(len());
    T s(rbuf+subTotal+rindex,len());
    rindex+=len();
    return s;
  }

  template<::std::size_t N>void copyString (char(&dest)[N]){
    MarshallingInt len(*this);
    if(len()+1>N)raise("ReceiveBuffer copyString");
    give(dest,len());
    dest[len()]='\0';
  }

  template<class T>void giveIlist (T& lst){
    for(int c=give<uint32_t>();c>0;--c)
      lst.push_back(*T::value_type::BuildPolyInstance(*this));
  }

  template<class T>void giveRbtree (T& rbt){
    auto endIt=rbt.end();
    for(int c=give<uint32_t>();c>0;--c)
      rbt.insert_unique(endIt,*T::value_type::BuildPolyInstance(*this));
  }
};

template<class T,class R>
T give (ReceiveBuffer<R>& buf){return buf.template give<T>();}

template<class R>bool giveBool (ReceiveBuffer<R>& buf){
  switch(buf.giveOne()){
    case 0:return false;
    case 1:return true;
    default:raise("giveBool");
  }
}

template<class R>auto giveString (ReceiveBuffer<R>& buf){
  return buf.template giveStringy<::std::string>();
}

template<class R>auto giveStringView (ReceiveBuffer<R>& buf){
  return buf.template giveStringy<::std::string_view>();
}
template<class R>auto giveStringView_plus (ReceiveBuffer<R>& buf){
  auto v=giveStringView(buf);
  buf.giveOne();
  return v;
}

class SendBuffer{
  SendBuffer (SendBuffer const&);
  SendBuffer& operator= (SendBuffer);
  int32_t savedSize=0;
protected:
  int index=0;
  int const bufsize;
  unsigned char* const buf;
public:
  sockType sock_=-1;

  SendBuffer (unsigned char* addr,int sz):bufsize(sz),buf(addr){}

  void checkSpace (int n){
    if(n>bufsize-index)raise("SendBuffer checkSpace",n,index);
  }

  void receive (void const* data,int size){
    checkSpace(size);
    ::memcpy(buf+index,data,size);
    index+=size;
  }

  template<class T>void receive (T t){
    static_assert(::std::is_arithmetic<T>::value||::std::is_enum<T>::value,"");
    receive(&t,sizeof t);
  }

  int reserveBytes (int n){
    checkSpace(n);
    auto i=index;
    index+=n;
    return i;
  }

  template<class T>T receive (int where,T t){
    static_assert(::std::is_arithmetic<T>::value,"");
    ::memcpy(buf+where,&t,sizeof t);
    return t;
  }

  void fillInSize (int32_t max){
    int32_t marshalledBytes=index-savedSize;
    if(marshalledBytes>max)raise("fillInSize",max);
    receive(savedSize,marshalledBytes);
    savedSize=index;
  }

  void reset (){savedSize=index=0;}
  void rollback (){index=savedSize;}

  void receiveFile (fileType d,int32_t sz){
    receive(sz);
    checkSpace(sz);
    if(Read(d,buf+index,sz)!=sz)raise("SendBuffer receiveFile");
    index+=sz;
  }

  bool flush (){
    int const bytes=sockWrite(sock_,buf,index);
    if(bytes==index){reset();return true;}

    index-=bytes;
    savedSize=index;
    ::memmove(buf,buf+bytes,index);
    return false;
  }

  //UDP-friendly alternative to flush
  void send (::sockaddr* addr=nullptr,::socklen_t len=0)
  {sockWrite(sock_,buf,index,addr,len);}

  unsigned char* data (){return buf;}
  int getIndex (){return index;}
  int getSize (){return bufsize;}
  template<class...T>void receiveMulti (char const*,T&&...);
};

inline void receive (SendBuffer&b,bool bl){b.receive(static_cast<unsigned char>(bl));}

inline void receive (SendBuffer& b,char const* s){
  MarshallingInt len(::strlen(s));
  len.marshal(b);
  b.receive(s,len());
}

inline void receive (SendBuffer& b,::std::string const& s){
  MarshallingInt len(s.size());
  len.marshal(b);
  b.receive(s.data(),len());
}

inline void receive (SendBuffer& b,::std::string_view const& s){
  MarshallingInt(s.size()).marshal(b);
  b.receive(s.data(),s.size());
}
using stringPlus=::std::initializer_list<::std::string_view>;
inline void receive (SendBuffer& b,stringPlus lst){
  int32_t t=0;
  for(auto s:lst)t+=s.size();
  MarshallingInt{t}.marshal(b);
  for(auto s:lst)b.receive(s.data(),s.size());//Use low-level receive
}

inline void insertNull (SendBuffer& b){uint8_t z=0;b.receive(z);}

template<class T>void receiveBlock (SendBuffer& b,T const& grp){
  int32_t count=grp.size();
  b.receive(count);
  if(count>0)
    b.receive(&*grp.begin(),count*sizeof(typename T::value_type));
}

template<class T>void receiveGroup (SendBuffer& b,T const& grp){
  b.receive(static_cast<int32_t>(grp.size()));
  for(auto const& e:grp)e.marshal(b);
}
template<class T>void receiveGroupPointer (SendBuffer& b,T const& grp){
  b.receive(static_cast<int32_t>(grp.size()));
  for(auto p:grp)p->marshal(b);
}

//Encode integer into variable-length format.
inline void MarshallingInt::marshal (SendBuffer& b)const{
  uint32_t n=val;
  for(;;){
    uint8_t a=n&127;
    n>>=7;
    if(0==n){b.receive(a);return;}
    b.receive(a|=128);
    --n;
  }
}

auto constexpr udp_packet_max=1280;
template<class R,int N=udp_packet_max>
struct BufferStack:SendBuffer,ReceiveBuffer<R>{
private:
  unsigned char ar[N];
public:
  BufferStack ():SendBuffer(ar,N),ReceiveBuffer<R>((char*)ar,0){}
  BufferStack (int s):BufferStack(){sock_=s;}

  bool getPacket (::sockaddr* addr=nullptr,::socklen_t* len=nullptr){
    this->packetLength=sockRead(sock_,ar,N,addr,len);
    return this->Update();
  }
};

template<class T>void reset (T* p){::memset(p,0,sizeof(T));}

struct SendBufferHeap:SendBuffer{
  SendBufferHeap (int sz):SendBuffer(new unsigned char[sz],sz){}
  ~SendBufferHeap (){delete[]buf;}
};

template<class R>struct BufferCompressed:SendBufferHeap,ReceiveBuffer<R>{
private:
  ::qlz_state_compress* compress;
  int const compSize;
  int compPacketSize;
  int compIndex=0;
  char* compBuf;
  bool kosher=true;

  ::qlz_state_decompress* decomp;
  int bytesRead=0;
  char* compressedStart;

  bool doFlush (){
    int const bytes=Write(sock_,compBuf,compIndex);
    if(bytes==compIndex){compIndex=0;return true;}
    compIndex-=bytes;
    ::memmove(compBuf,compBuf+bytes,compIndex);
    return false;
  }
public:
  BufferCompressed (int sz,int d):SendBufferHeap(sz),ReceiveBuffer<R>(new char[sz],0)
              ,compress(nullptr),compSize(sz+(sz>>3)+400),compBuf(nullptr)
              ,decomp(nullptr){(void)d;}

  explicit BufferCompressed (int sz):BufferCompressed(sz,0){
    compress=new ::qlz_state_compress();
    compBuf=new char[compSize];
    decomp=new ::qlz_state_decompress();
  }

  ~BufferCompressed (){
    delete[]rbuf;
    delete compress;
    delete[]compBuf;
    delete decomp;
  }

  bool flush (){
    bool rc=true;
    if(compIndex>0)rc=doFlush();

    if(index>0){
      if(index+(index>>3)+400>compSize-compIndex)
        raise("Not enough room in compressed buf");
      compIndex+=::qlz_compress(buf,compBuf+compIndex,index,compress);
      reset();
      if(rc)rc=doFlush();
    }
    return rc;
  }

  void compressedReset (){
    reset();
    ::cmw::reset(compress);
    ::cmw::reset(decomp);
    compIndex=bytesRead=0;
    kosher=true;
  }

  using ReceiveBuffer<R>::rbuf;
  bool gotPacket (){
    if(kosher){
      if(bytesRead<9){
        bytesRead+=Read(sock_,rbuf+bytesRead,9-bytesRead);
        if(bytesRead<9)return false;
        if((compPacketSize=::qlz_size_compressed(rbuf))>bufsize||
           (this->packetLength=::qlz_size_decompressed(rbuf))>bufsize){
          kosher=false;
          raise("gotPacket too big",compPacketSize,this->packetLength,bufsize);
        }
        compressedStart=rbuf+bufsize-compPacketSize;
        ::memmove(compressedStart,rbuf,9);
      }
      bytesRead+=Read(sock_,compressedStart+bytesRead,compPacketSize-bytesRead);
      if(bytesRead<compPacketSize)return false;
      ::qlz_decompress(compressedStart,rbuf,decomp);
      bytesRead=0;
      this->Update();
      return true;
    }
    bytesRead+=Read(sock_,rbuf,::std::min(bufsize,compPacketSize-bytesRead));
    if(bytesRead==compPacketSize){
      kosher=true;
      bytesRead=0;
    }
    return false;
  }
};

template<int N>class FixedString{
  MarshallingInt len;
  ::std::array<char,N> str;
 public:
  FixedString (){}

  explicit FixedString (char const* s):len(::strlen(s)){
    if(len()>N-1)raise("FixedString ctor");
    ::strcpy(&str[0],s);
  }

  explicit FixedString (::std::string_view s):len(s.length()){
    if(len()>N-1)raise("FixedString ctor");
    ::strncpy(&str[0],s.data(),len());
    str[len()]='\0';
  }

  template<class R>explicit FixedString (ReceiveBuffer<R>& b):len(b){
    if(len()>N-1)raise("FixedString stream ctor");
    b.give(&str[0],len());
    str[len()]='\0';
  }

  void marshal (SendBuffer& b)const{
    len.marshal(b);
    b.receive(&str[0],len());
  }

  int bytesAvailable (){return N-(len()+1);}

  char* operator() (){return &str[0];}
  char const* c_str ()const{return &str[0];}
  char operator[] (int i)const{return str[i];}
};
using FixedString60=FixedString<60>;
using FixedString120=FixedString<120>;
}
#endif
