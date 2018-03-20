#pragma once
#include"ErrorWords.hh"
#include"quicklz.h"
#include<array>
#include<initializer_list>
#include<limits>
#include<string>
#if __cplusplus>=201703L||_MSVC_LANG>=201403L
#include<string_view>
#endif
#include<type_traits>
static_assert(::std::numeric_limits<unsigned char>::digits==8,"");
static_assert(::std::numeric_limits<float>::is_iec559
              ,"Only IEEE 754 supported");

#include<assert.h>
#include<stdint.h>
#include<stdio.h>//snprintf
#include<stdlib.h>//strtol
#include<string.h>//memcpy,strlen
#ifndef CMW_WINDOWS
#include<fcntl.h>//open
#include<sys/socket.h>
#include<sys/stat.h>//open
#include<sys/types.h>
#include<unistd.h>//close,read,write
#endif

namespace cmw{
class SendBuffer;
template<class R> class ReceiveBuffer;

class marshallingInt{
  int32_t val;
public:
  inline marshallingInt (){}
  inline explicit marshallingInt (int32_t v):val(v){}
  inline explicit marshallingInt (char const* v):val(::strtol(v,0,10)){}

  //Reads a sequence of bytes in variable-length format and
  //composes a 32 bit integer.
  template<class R>
  explicit marshallingInt (ReceiveBuffer<R>& b):val(0){
    uint32_t shift=1;
    for(;;){
      uint8_t a=b.GiveOne();
      val+=(a&127)*shift;
      if((a&128)==0)return;
      shift<<=7;
      val+=shift;
    }
  }

  inline void operator= (int32_t r){val=r;}
  inline int32_t operator() ()const{return val;}
  inline void Marshal (SendBuffer&)const;
};

inline bool operator== (marshallingInt l,marshallingInt r){return l()==r();}
inline bool operator== (marshallingInt l,int32_t r){return l()==r;}

inline int sockWrite (sockType s,void const* data,int len
                      ,sockaddr* addr=nullptr,socklen_t toLen=0){
  int rc=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);
  if(rc>0)return rc;
  auto err=GetError();
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockWrite:")<<s<<" "<<err;}

inline int sockRead (sockType s,char* data,int len
                     ,sockaddr* addr=nullptr,socklen_t* fromLen=nullptr){
  int rc=::recvfrom(s,data,len,0,addr,fromLen);
  if(rc>0)return rc;
  if(rc==0)throw connectionLost("sockRead eof:")<<s<<" "<<len;
  auto err=GetError();
  if(ECONNRESET==err)throw connectionLost("sockRead-ECONNRESET");
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockRead:")<<s<<" "<<len<<" "<<err;
}

#ifdef CMW_WINDOWS
inline auto Write (HANDLE h,void const* data,int len){
  DWORD bytesWritten=0;
  if(!WriteFile(h,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    throw failure("Write ")<<GetLastError();
  return bytesWritten;
}

inline auto Read (HANDLE h,void* data,int len){
  DWORD bytesRead=0;
  if (!ReadFile(h,static_cast<char*>(data),len,&bytesRead,nullptr))
    throw failure("Read ")<<GetLastError();
  return bytesRead;
}
#else
inline int Write (int fd,void const* data,int len){
  int rc=::write(fd,data,len);
  if(rc>=0)return rc;
  throw failure("Write ")<<errno;
}

inline int Read (int fd,void* data,int len){
  int rc=::read(fd,data,len);
  if(rc>0)return rc;
  if(rc==0)throw connectionLost("Read eof:")<<len;
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  throw failure("Read:")<<len<<" "<<errno;
}
#endif

struct SameFormat{
  template<template<class> class B,class U>
  void Read (B<SameFormat>& buf,U& data){buf.Give(&data,sizeof(U));}

  template<template<class> class B,class U>
  void ReadBlock (B<SameFormat>& buf,U* data,int elements)
  {buf.Give(data,elements*sizeof(U));}
};

struct LeastSignificantFirst{
  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint8_t& val)
  {val=buf.GiveOne();}

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint16_t& val){
    val=buf.GiveOne();
    val|=buf.GiveOne()<<8;
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint32_t& val){
    val=buf.GiveOne();
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<24;
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,uint64_t& val){
    val=buf.GiveOne();
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<24;
    val|=(uint64_t)buf.GiveOne()<<32;
    val|=(uint64_t)buf.GiveOne()<<40;
    val|=(uint64_t)buf.GiveOne()<<48;
    val|=(uint64_t)buf.GiveOne()<<56;
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,float& f){
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&f,&tmp,sizeof(f));
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,double& d){
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&d,&tmp,sizeof(d));
  }

  template<template<class> class B,class U>
  void ReadBlock (B<LeastSignificantFirst>& buf
                  ,U* data,int elements){
    for(int i=0;i<elements;++i){*(data+i)=buf.template Give<U>();}
  }

  //Overloads for uint8_t and int8_t
  template<template<class> class B>
  void ReadBlock (B<LeastSignificantFirst>& buf
                  ,uint8_t* data,int elements){
    buf.Give(data,elements);
  }

  template<template<class> class B>
  void ReadBlock (B<LeastSignificantFirst>& buf
                  ,int8_t* data,int elements){
    buf.Give(data,elements);
  }
};

struct MostSignificantFirst{
  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint8_t& val)
  {val=buf.GiveOne();}

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint16_t& val){
    val=buf.GiveOne()<<8;
    val|=buf.GiveOne();
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint32_t& val){
    val=buf.GiveOne()<<24;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne();
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,uint64_t& val){
    val=(uint64_t)buf.GiveOne()<<56;
    val|=(uint64_t)buf.GiveOne()<<48;
    val|=(uint64_t)buf.GiveOne()<<40;
    val|=(uint64_t)buf.GiveOne()<<32;
    val|=buf.GiveOne()<<24;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne();
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,float& f){
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&f,&tmp,sizeof(f));
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,double& d){
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&d,&tmp,sizeof(d));
  }

  template<template<class> class B,class U>
  void ReadBlock (B<MostSignificantFirst>& buf
                  ,U* data,int elements){
    for(int i=0;i<elements;++i){*(data+i)=buf.template Give<U>();}
  }

  template<template<class> class B>
  void ReadBlock (B<MostSignificantFirst>& buf
                  ,uint8_t* data,int elements){
    buf.Give(data,elements);
  }

  template<template<class> class B>
  void ReadBlock (B<MostSignificantFirst>& buf
                  ,int8_t* data,int elements){
    buf.Give(data,elements);
  }
};


template<class R>
class ReceiveBuffer{
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
    if(n>msgLength-rindex)throw
      failure("ReceiveBuffer checkData:")<<n<<" "<<msgLength<<" "<<rindex;
  }

  void Give (void* address,int len){
    checkData(len);
    ::memcpy(address,rbuf+subTotal+rindex,len);
    rindex+=len;
  }

  char GiveOne (){
    checkData(1);
    return rbuf[subTotal+rindex++];
  }

  template<class T>
  T Give (){
    T tmp;
    reader.Read(*this,tmp);
    return tmp;
  }

  bool NextMessage (){
    subTotal+=msgLength;
    if(subTotal<packetLength){
      rindex=0;
      msgLength=sizeof(int32_t);
      msgLength=Give<uint32_t>();
      return true;
    }
    return false;
  }

  void Update (){
    msgLength=subTotal=0;
    NextMessage();
  }

  template<class T>
  void GiveBlock (T* data,unsigned int elements)
  {reader.ReadBlock(*this,data,elements);}

  void GiveFile (fileType d){
    int sz=Give<uint32_t>();
    checkData(sz);
    while(sz>0){
      int rc=Write(d,rbuf+subTotal+rindex,sz);
      sz-=rc;
      rindex+=rc;
    }
  }

  template<class T>
  T GiveStringy (){
    marshallingInt len(*this);
    checkData(len());
    T s(rbuf+subTotal+rindex,len());
    rindex+=len();
    return s;
  }

#ifndef CMW_WINDOWS
  template<ssize_t N>
  void CopyString (char (&dest)[N]){
    marshallingInt len(*this);
    if(len()+1>N)throw failure("ReceiveBuffer::CopyString");
    Give(dest,len());
    dest[len()]='\0';
  }
#endif

  template<class T>
  void Giveilist (T& lst){
    for(int count=Give<uint32_t>();count>0;--count)
      lst.push_back(*T::value_type::BuildPolyInstance(*this));
  }

  template<class T>
  void Giverbtree (T& rbt){
    auto endIt=rbt.end();
    for(int count=Give<uint32_t>();count>0;--count)
      rbt.insert_unique(endIt,*T::value_type::BuildPolyInstance(*this));
  }

private:
  ReceiveBuffer (ReceiveBuffer const&);
  ReceiveBuffer& operator= (ReceiveBuffer);
};

template<class T,class R>
T Give (ReceiveBuffer<R>& buf){return buf.template Give<T>();}

template<class R>
bool GiveBool (ReceiveBuffer<R>& buf){return buf.GiveOne()!=0;}

template<class R>
::std::string GiveString (ReceiveBuffer<R>& buf){
  return buf.template GiveStringy<::std::string>();
}

#if __cplusplus>=201703L||_MSVC_LANG>=201403L
template<class R>
auto GiveStringView (ReceiveBuffer<R>& buf){
  return buf.template GiveStringy<::std::string_view>();
}

template<class R>
auto GiveStringView_plus (ReceiveBuffer<R>& buf){
  auto v=GiveStringView(buf);
  buf.GiveOne();
  return v;
}
#endif

class SendBuffer{
  SendBuffer (SendBuffer const&);
  SendBuffer& operator= (SendBuffer);
  int32_t savedSize=0;
protected:
  int index=0;
  int const bufsize;
  unsigned char* buf;

public:
  sockType sock_=-1;

  inline SendBuffer (unsigned char* addr,int sz):bufsize(sz),buf(addr){}

  inline void checkSpace (int n){
    if(n>bufsize-index)throw failure("SendBuffer checkSpace:")<<n<<" "<<index;
  }

  inline void Receive (void const* data,int size){
    checkSpace(size);
    ::memcpy(buf+index,data,size);
    index+=size;
  }

  template<class T>
  void Receive (T t){
    static_assert(::std::is_arithmetic<T>::value,"");
    Receive(&t,sizeof(T));
  }

  inline int ReserveBytes (int n){
    checkSpace(n);
    auto i=index;
    index+=n;
    return i;
  }

  template<class T>
  void Receive (int where,T t){
    static_assert(::std::is_arithmetic<T>::value,"");
    ::memcpy(buf+where,&t,sizeof(T));
  }

  inline void FillInSize (int32_t max){
    int32_t marshalledBytes=index-savedSize;
    if(marshalledBytes>max)
      throw failure("Size of marshalled data exceeds max ")<<max;

    Receive(savedSize,marshalledBytes);
    savedSize=index;
  }

  inline void Reset (){savedSize=index=0;}
  inline void Rollback (){index=savedSize;}

  template<typename... T>
  void Receive_variadic (char const* format,T&&... t){
    auto max=bufsize-index;
    auto size=::snprintf((char*)buf+index,max,format,t...);
    if(size>max)throw failure("SendBuffer Receive_variadic");
    index+=size;
  }

  inline void ReceiveFile (fileType d,int32_t sz){
    Receive(sz);
    checkSpace(sz);
    if(Read(d,buf+index,sz)!=sz)throw failure("SendBuffer ReceiveFile");
    index+=sz;
  }

  inline bool Flush (::sockaddr* addr=nullptr,::socklen_t len=0){
    int const bytes=sockWrite(sock_,buf,index,addr,len);
    if(bytes==index){Reset();return true;}

    index-=bytes;
    savedSize=index;
    ::memmove(buf,buf+bytes,index);
    return false;
  }

  //UDP-friendly alternative to Flush
  inline void Send (::sockaddr* addr=nullptr,::socklen_t len=0)
  {sockWrite(sock_,buf,index,addr,len);}

  inline unsigned char* data (){return buf;}
  inline int GetIndex (){return index;}
};

inline void Receive (SendBuffer&b,bool bl){b.Receive(static_cast<unsigned char>(bl));}

inline void Receive (SendBuffer& b,char const* s){
  marshallingInt len(::strlen(s));
  len.Marshal(b);
  b.Receive(s,len());
}

inline void Receive (SendBuffer& b,::std::string const& s){
  marshallingInt len(s.size());
  len.Marshal(b);
  b.Receive(s.data(),len());
}

#if __cplusplus>=201703L||_MSVC_LANG>=201403L
inline void Receive (SendBuffer& b,::std::string_view const& s){
  marshallingInt(s.size()).Marshal(b);
  b.Receive(s.data(),s.size());
}

using stringPlus=::std::initializer_list<::std::string_view>;
inline void Receive (SendBuffer& b,stringPlus lst){
  int32_t t=0;
  for(auto s:lst)t+=s.size();
  marshallingInt{t}.Marshal(b);
  for(auto s:lst)b.Receive(s.data(),s.size());//Use low-level Receive
}
#endif

inline void InsertNull (SendBuffer& b){uint8_t z=0;b.Receive(z);}

template<class T>
void ReceiveBlock (SendBuffer& b,T const& grp){
  int32_t count=grp.size();
  b.Receive(count);
  if(count>0)
    b.Receive(&*grp.begin(),count*sizeof(typename T::value_type));
}

template<class T>
void ReceiveGroup (SendBuffer& b,T const& grp){
  b.Receive(static_cast<int32_t>(grp.size()));
  for(auto const& e:grp)e.Marshal(b);
}

template<class T>
void ReceiveGroupPointer (SendBuffer& b,T const& grp){
  b.Receive(static_cast<int32_t>(grp.size()));
  for(auto p:grp)p->Marshal(b);
}

//Encode integer into variable-length format.
void marshallingInt::Marshal (SendBuffer& b)const{
  uint32_t n=val;
  for(;;){
    uint8_t a=n&127;
    n>>=7;
    if(0==n){b.Receive(a);return;}
    b.Receive(a|=128);
    --n;
  }
}


auto const udp_packet_max=1280;
template<class R,int N=udp_packet_max>
struct BufferStack:SendBuffer,ReceiveBuffer<R>{
private:
  unsigned char ar[N];
  char ar2[N];

public:
  BufferStack ():SendBuffer(ar,N),ReceiveBuffer<R>(ar2,0){}

  void GetPacket (::sockaddr* addr=nullptr,::socklen_t* len=nullptr){
    this->packetLength=sockRead(sock_,ar2,N,addr,len);
    this->Update();
  }
};

template<typename T>
void reset (T* p){::memset(p,0,sizeof(T));}

struct SendBufferHeap:SendBuffer{
  inline SendBufferHeap (int sz):SendBuffer(new unsigned char[sz],sz){}
  inline ~SendBufferHeap (){delete[]buf;}
};

template<typename R>
struct BufferCompressed:SendBufferHeap,ReceiveBuffer<R>{
private:
  ::qlz_state_compress* compress;
  int const compSize;
  int compIndex=0;
  char* compBuf;

  ::qlz_state_decompress* decomp;
  int bytesRead=0;
  char* compressedStart;

  bool doFlush (::sockaddr* addr,::socklen_t len){
    int const bytes=sockWrite(sock_,compBuf,compIndex,addr,len);
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
    delete[]this->rbuf;
    delete compress;
    delete[]compBuf;
    delete decomp;
  }

  bool Flush (::sockaddr* addr=nullptr,::socklen_t len=0){
    bool rc=true;
    if(compIndex>0)rc=doFlush(addr,len);

    if(index>0){
      if(index+(index>>3)+400>compSize-compIndex)
        throw failure("Not enough room in compressed buf");
      compIndex+=::qlz_compress(buf,compBuf+compIndex,index,compress);
      Reset();
      if(rc)rc=doFlush(addr,len);
    }
    return rc;
  }

  void CompressedReset (){
    Reset();
    reset(compress);
    reset(decomp);
    compIndex=bytesRead=0;
  }

  using ReceiveBuffer<R>::rbuf;
  bool GotPacket (){
    try{
      static int compressedSize;
      if(bytesRead<9){
        bytesRead+=sockRead(sock_,rbuf+bytesRead,9-bytesRead);
        if(bytesRead<9)return false;

        if((this->packetLength=::qlz_size_decompressed(rbuf))>bufsize)
          throw failure("Decompress::GotPacket decompressed size too big ")
                         <<this->packetLength<<" "<<bufsize;

        if((compressedSize=::qlz_size_compressed(rbuf))>bufsize)
          throw failure("Decompress::GotPacket compressed size too big");

        compressedStart=rbuf+bufsize-compressedSize;
        ::memmove(compressedStart,rbuf,9);
      }
      bytesRead+=sockRead(sock_,compressedStart+bytesRead
                          ,compressedSize-bytesRead);
      if(bytesRead<compressedSize)return false;

      ::qlz_decompress(compressedStart,rbuf,decomp);
      bytesRead=0;
      this->Update();
      return true;
    }catch(...){
      bytesRead=0;
      throw;
    }
  }
};

template<int N>
class fixedString{
  marshallingInt len;
  ::std::array<char,N> str;

 public:
  fixedString ()=default;

  explicit fixedString (char const* s):len(::strlen(s)){
    if(len()>N-1)throw failure("fixedString ctor");
    ::strcpy(&str[0],s);
  }

#if __cplusplus>=201703L||_MSVC_LANG>=201403L
  explicit fixedString (::std::string_view s):len(s.length()){
    if(len()>N-1)throw failure("fixedString ctor");
    ::strncpy(&str[0],s.data(),len());
    str[len()]='\0';
  }
#endif

  template<class R>
  explicit fixedString (ReceiveBuffer<R>& b):len(b){
    if(len()>N-1)throw failure("fixedString stream ctor");
    b.Give(&str[0],len());
    str[len()]='\0';
  }

  void Marshal (SendBuffer& b)const{
    len.Marshal(b);
    b.Receive(&str[0],len());
  }

  int bytesAvailable (){return N-(len()+1);}

  char* operator() (){return &str[0];}
  char const* c_str ()const{return &str[0];}
  char operator[] (int i)const{return str[i];}
};

using fixedString60=fixedString<60>;
using fixedString120=fixedString<120>;

#if __cplusplus>=201703L||_MSVC_LANG>=201403L
class File{
  ::std::string_view name;
public:
  inline explicit File (::std::string_view n):name(n){}

  template<class R>
  explicit File (ReceiveBuffer<R>& buf):name(GiveStringView_plus(buf)){
    int d=::open(name.data(),O_WRONLY|O_CREAT|O_TRUNC
                 ,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if(d<0)throw failure("File open ")<<name.data()<<" "<<errno;
    try{buf.GiveFile(d);}catch(...){::close(d);throw;}
    ::close(d);
  }

  inline char const* Name ()const{return name.data();}
};

template<class R>
void GiveFiles (ReceiveBuffer<R>& b){
  for(auto n=marshallingInt{b}();n>0;--n)File{b};
}
#endif


template<typename C>
int32_t MarshalSegment (C&,uint8_t&,SendBuffer&)
{return 0;}

template<typename T,typename... Ts,typename C>
int32_t MarshalSegment (C& c,uint8_t& segments,SendBuffer& buf){
  int32_t total=0;
  if(c.template is_registered<T>()){
    total=c.template size<T>();
    if(total>0){
      ++segments;
      buf.Receive(T::typeNum);
      buf.Receive(total);
      for(T const& t:c.template segment<T>()){t.Marshal(buf);}
    }
  }
  return total+MarshalSegment<Ts...>(c,segments,buf);
}

template<typename... Ts,typename C>
void MarshalCollection (C& c,SendBuffer& buf){
  auto ind=buf.ReserveBytes(1);
  uint8_t segments=0;
  assert(c.size()==MarshalSegment<Ts...>(c,segments,buf));
  buf.Receive(ind,segments);
}

template<typename T,typename C,typename R>
void BuildSegment (C& c,ReceiveBuffer<R>& buf){
  int32_t n=Give<int32_t>(buf);
  c.template reserve<T>(n);
  for(;n>0;--n){c.template emplace<T>(buf);}
}

template<typename C,typename R,typename F>
void BuildCollection (C& c,ReceiveBuffer<R>& buf,F f){
  for(auto segs=Give<uint8_t>(buf);segs>0;--segs){f(c,buf);}
}
}

using messageID_8=uint8_t;
using messageID_16=uint16_t;
