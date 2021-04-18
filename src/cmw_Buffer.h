#ifndef CMW_BUFFER_H
#define CMW_BUFFER_H
#include<quicklz.h>
#include<exception>
#include<initializer_list>
#include<span>
#include<string>
#include<string_view>
#include<type_traits>
#include<stdint.h>
#include<stdio.h>//fopen,snprintf
#include<string.h>//memcpy,memmove

#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#include<ws2tcpip.h>
#define CMW_WINDOWS
#else
#include<fcntl.h>//fcntl,open
#include<sys/socket.h>
#include<sys/stat.h>//open
#include<sys/types.h>
#include<syslog.h>
#endif

struct pollfd;
struct addrinfo;
namespace cmw{
class Failure:public ::std::exception{
  ::std::string s;
 public:
  explicit Failure (char const *s):s(s){}
  void operator<< (::std::string_view v){s.append(" "); s.append(v);}
  void operator<< (int i){char b[12]; ::snprintf(b,sizeof b,"%d",i);*this<<b;}
  char const* what ()const noexcept{return s.data();}
};

struct Fiasco:Failure{explicit Fiasco (char const *s):Failure{s}{}};

template<class E>void apps (E&){}
template<class E,class T,class...Ts>void apps (E& e,T t,Ts...ts){
  e<<t; apps(e,ts...);
}

template<class E=Failure,class...T>[[noreturn]]void raise (char const *s,T...t){
  E e{s}; apps(e,t...); throw e;
}

int getError ();
void winStart ();
int fromChars (::std::string_view);
void setDirectory (char const*);

class SendBuffer;
template<class>class ReceiveBuffer;
class MarshallingInt{
  ::int32_t val;
 public:
  MarshallingInt (){}
  explicit MarshallingInt (::int32_t v):val{v}{}
  explicit MarshallingInt (::std::string_view v):val{fromChars(v)}{}

  //Reads a sequence of bytes in variable-length format and
  //builds a 32 bit integer.
  template<class R>explicit MarshallingInt (ReceiveBuffer<R>& b):val(0){
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
  void marshal (SendBuffer&)const;
};
inline bool operator== (MarshallingInt l,MarshallingInt r){return l()==r();}
inline bool operator== (MarshallingInt l,::int32_t r){return l()==r;}

#ifdef CMW_WINDOWS
using sockType=SOCKET;
using fileType=HANDLE;
DWORD Write (HANDLE,void const*,int);
DWORD Read (HANDLE,void*,int);
#else
using sockType=int;
using fileType=int;
int Write (int,void const*,int);
int Read (int,void*,int);

void exitFailure ();

template<class...T>void bail (char const *fmt,T... t)noexcept{
  ::syslog(LOG_ERR,fmt,t...);
  exitFailure();
}

struct FileWrapper{
  int const d=-2;
  FileWrapper (){}
  FileWrapper (char const *name,int flags,mode_t=0);
  FileWrapper (FileWrapper const&)=delete;
  ~FileWrapper ();
};

auto getFile =[](char const *n,auto& b){
  b.giveFile(FileWrapper{n,O_WRONLY|O_CREAT|O_TRUNC
                         ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH}.d);
};

class File{
  char const *nam;
 public:
  explicit File (char const *n):nam{n}{}

  template<class R>
  explicit File (ReceiveBuffer<R>& b):nam{b.giveStringView().data()}{
    getFile(nam,b);
  }

  auto name ()const{return nam;}
};
#endif

auto setsockWrapper =[](sockType s,int opt,auto t){
  return ::setsockopt(s,SOL_SOCKET,opt,reinterpret_cast<char*>(&t),sizeof t);
};

void setRcvTimeout (sockType,int);
int setNonblocking (sockType);
void closeSocket (sockType);
int preserveError (sockType);
int pollWrapper (::pollfd*,int,int=-1);

class GetaddrinfoWrapper{
  ::addrinfo *head,*addr;
 public:
  GetaddrinfoWrapper (char const *node,char const *port,int type,int flags=0);

  ~GetaddrinfoWrapper ();
  auto& operator() (){return *addr;}
  void inc ();

  sockType getSock ();
  GetaddrinfoWrapper (GetaddrinfoWrapper const&)=delete;
  GetaddrinfoWrapper& operator= (GetaddrinfoWrapper)=delete;
};

sockType connectWrapper (char const *node,char const *port);
sockType udpServer (char const *port);
sockType tcpServer (char const *port);
int acceptWrapper (sockType);

int sockWrite (sockType,void const *data,int len
               ,::sockaddr const* =nullptr,::socklen_t=0);

int sockRead (sockType,void *data,int len,::sockaddr*,::socklen_t*);

struct SameFormat{
  template<class B,class U>static void read (B& b,U& data)
  {b.give(&data,sizeof(U));}

  template<class B,class U>static void readBlock (B& b,U *data,int elements)
  {b.give(data,elements*sizeof(U));}
};

struct LeastSignificantFirst{
  template<class B,class U>static void read (B& b,U& val)
  {for(auto c=0;c<sizeof(U);++c)val|=b.giveOne()<<8*c;}

  template<class B>static void read (B& b,float& f){
    ::uint32_t tmp;
    read(b,tmp);
    ::memcpy(&f,&tmp,sizeof f);
  }

  template<class B>static void read (B& b,double& d){
    ::uint64_t tmp;
    read(b,tmp);
    ::memcpy(&d,&tmp,sizeof d);
  }

  template<class B,class U>static void readBlock (B& b,U *data,int elements){
    for(;elements>0;--elements){*data++=b.template give<U>();}
  }
};

struct MostSignificantFirst{
  template<class B,class U>static void read (B& b,U& val)
  {for(auto c=sizeof(U);c>0;--c)val|=b.giveOne()<<8*(c-1);}

  template<class B>static void read (B& b,float& f){
    ::uint32_t tmp;
    read(b,tmp);
    ::memcpy(&f,&tmp,sizeof f);
  }

  template<class B> static void read (B& b,double& d){
    ::uint64_t tmp;
    read(b,tmp);
    ::memcpy(&d,&tmp,sizeof d);
  }

  template<class B,class U>static void readBlock (B& b,U *data,int elements){
    for(;elements>0;--elements){*data++=b.template give<U>();}
  }
};

template<class R> class ReceiveBuffer{
  int msgLength=0;
  int subTotal=0;
 protected:
  int rindex=0;
  int packetLength;
  char* const rbuf;

 public:
  ReceiveBuffer (char *addr,int bytes):packetLength{bytes},rbuf{addr}{}

  void checkLen (int n){
    if(n>msgLength-rindex)raise("ReceiveBuffer checkLen",n,msgLength,rindex);
  }

  void give (void *address,int len){
    checkLen(len);
    ::memcpy(address,rbuf+subTotal+rindex,len);
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
      msgLength=sizeof(::int32_t);
      msgLength=give<::uint32_t>();
      return true;
    }
    return false;
  }

  bool update (){
    msgLength=subTotal=0;
    return nextMessage();
  }

  template<class T>void giveBlock (T *data,unsigned int elements){
    if(sizeof(T)==1)give(data,elements);
    else R::readBlock(*this,data,elements);
  }

  void giveFile (fileType d){
    int sz=give<::uint32_t>();
    checkLen(sz);
    while(sz>0){
      int r=Write(d,rbuf+subTotal+rindex,sz);
      sz-=r;
      rindex+=r;
    }
  }

  template<class T>auto giveSpan (){
    static_assert(::std::is_arithmetic_v<T>);
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

  template<class T>void giveIlist (T& lst){
    for(int c=give<::uint32_t>();c>0;--c)
      lst.push_back(*T::value_type::buildPolyInstance(*this));
  }

  template<class T>void giveRbtree (T& rbt){
    auto endIt=rbt.end();
    for(int c=give<::uint32_t>();c>0;--c)
      rbt.insert_unique(endIt,*T::value_type::buildPolyInstance(*this));
  }
};

template<class T,class R>T give (ReceiveBuffer<R>& b)
{return b.template give<T>();}

template<class R>bool giveBool (ReceiveBuffer<R>& b){
  switch(b.giveOne()){
    case 0:return false;
    case 1:return true;
    default:raise("giveBool");
  }
}

class SendBuffer{
  SendBuffer (SendBuffer const&)=delete;
  SendBuffer& operator= (SendBuffer);
  ::int32_t savedSize=0;
 protected:
  int index=0;
  int const bufsize;
  unsigned char* const buf;
 public:
  sockType sock_=-1;

  SendBuffer (unsigned char *addr,int sz):bufsize(sz),buf(addr){}

  int reserveBytes (int n);

  void receive (void const *data,int size){
    auto prev=reserveBytes(size);
    ::memcpy(buf+prev,data,size);
  }

  template<class T>void receive (T t){
    static_assert(::std::is_arithmetic_v<T>||::std::is_enum_v<T>);
    receive(&t,sizeof t);
  }

  template<class T>T receive (int where,T t){
    static_assert(::std::is_arithmetic_v<T>);
    ::memcpy(buf+where,&t,sizeof t);
    return t;
  }

  void fillInSize (::int32_t max);

  void reset (){savedSize=index=0;}
  void rollback (){index=savedSize;}

  void receiveFile (fileType,::int32_t sz);
  bool flush ();

  //UDP-friendly alternative to flush
  void send (::sockaddr *addr=nullptr,::socklen_t len=0)
  {sockWrite(sock_,buf,index,addr,len);}

  unsigned char* data (){return buf;}
  int getIndex (){return index;}
  int getSize (){return bufsize;}
  template<class...T>void receiveMulti (char const*,T...);
};

void receiveBool (SendBuffer&,bool);
void receive (SendBuffer&,::std::string_view);
void receiveNull (SendBuffer&,char const*);

using stringPlus=::std::initializer_list<::std::string_view>;
void receive (SendBuffer&,stringPlus);

template<template<class...>class C,class T>
void receiveBlock (SendBuffer& b,C<T>const& c){
  ::int32_t n=c.size();
  b.receive(n);
  if constexpr(::std::is_arithmetic_v<T>){
    if(n>0)b.receive(c.data(),n*sizeof(T));
  }else if constexpr(::std::is_pointer_v<T>)for(auto e:c)e->marshal(b);
  else for(auto const& e:c)e.marshal(b);
}

inline constexpr auto udpPacketMax=1280;
template<class R,int N=udpPacketMax>
class BufferStack:public SendBuffer,public ReceiveBuffer<R>{
  unsigned char ar[N];
 public:
  BufferStack ():SendBuffer(ar,N),ReceiveBuffer<R>((char*)ar,0){}
  BufferStack (int s):BufferStack(){sock_=s;}

  bool getPacket (::sockaddr *addr=nullptr,::socklen_t *len=nullptr){
    this->packetLength=sockRead(sock_,ar,N,addr,len);
    return this->update();
  }
};

auto clear =[](auto p){::memset(p,0,sizeof *p);};

struct SendBufferHeap:SendBuffer{
  SendBufferHeap (int n):SendBuffer(new unsigned char[n],n){}
  ~SendBufferHeap (){delete[]buf;}
};

#ifndef CMW_WINDOWS
template<class T>T const& myMin (T const& a,T const& b){return a<b?a:b;}

template<class R>struct BufferCompressed:SendBufferHeap,ReceiveBuffer<R>{
 private:
  ::qlz_state_compress *compress=nullptr;
  int const compSize;
  int compPacketSize;
  int compIndex=0;
  char *compBuf=nullptr;
  bool kosher=true;

  ::qlz_state_decompress *decomp=nullptr;
  int bytesRead=0;
  char *compressedStart;

  bool doFlush (){
    int const bytes=Write(sock_,compBuf,compIndex);
    if(bytes==compIndex){compIndex=0;return true;}
    compIndex-=bytes;
    ::memmove(compBuf,compBuf+bytes,compIndex);
    return false;
  }
 public:
  BufferCompressed (int sz,int):SendBufferHeap(sz),ReceiveBuffer<R>(new char[sz],0)
                     ,compSize(sz+(sz>>3)+400){}

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
    clear(compress);
    clear(decomp);
    compIndex=bytesRead=0;
    kosher=true;
    closeSocket(sock_);
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
      this->update();
      return true;
    }
    bytesRead+=Read(sock_,rbuf,myMin(bufsize,compPacketSize-bytesRead));
    if(bytesRead==compPacketSize){
      kosher=true;
      bytesRead=0;
    }
    return false;
  }
};
#endif

template<int N>class FixedString{
  MarshallingInt len{0};
  char str[N];

 public:
  FixedString (){}

  explicit FixedString (::std::string_view s):len(s.size()){
    if(len()>=N)raise("FixedString ctor");
    ::memcpy(str,s.data(),len());
    str[len()]=0;
  }

  template<class R>explicit FixedString (ReceiveBuffer<R>& b):len(b){
    if(len()>=N)raise("FixedString stream ctor");
    b.give(str,len());
    str[len()]=0;
  }

  void marshal (SendBuffer& b)const{
    len.marshal(b);
    b.receive(str,len());
  }

  unsigned int bytesAvailable ()const{return N-(len()+1);}

  void append (::std::string_view s){
    if(bytesAvailable()>=s.size()){
      ::memcpy(str+len(),s.data(),s.size());
      len+=s.size();
      str[len()]=0;
    }
  }

  char* operator() (){return str;}
  char const* data ()const{return str;}
  char operator[] (int i)const{return str[i];}
};
using FixedString60=FixedString<60>;
using FixedString120=FixedString<120>;

struct FILEwrapper{
  FILE* const hndl;
  char line[120];

  FILEwrapper (char const *n,char const *mode);
  FILEwrapper (FILEwrapper const&)=delete;
  ~FILEwrapper ();
  char* fgets ();
};
}
#endif
