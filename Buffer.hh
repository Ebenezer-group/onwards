#pragma once
#include"ErrorWords.hh"
#include"quicklz.h"
#include<array>
#include<initializer_list>
#include<limits>
#include<string>
#include<string_view>
#include<type_traits>
static_assert(::std::numeric_limits<unsigned char>::digits==8);
static_assert(::std::numeric_limits<float>::is_iec559
              ,"Only IEEE 754 supported");

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
  inline auto operator() ()const{return val;}
  inline void Marshal (SendBuffer&,bool=false)const;
};

inline bool operator== (marshallingInt l,marshallingInt r){return l()==r();}
inline bool operator== (marshallingInt l,int32_t r){return l()==r;}

inline int sockWrite (sockType s,void const* data,int len
                      ,sockaddr* addr=nullptr,socklen_t toLen=0){
  int rc=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);
  if(rc>0)return rc;
  auto err=GetError();
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockWrite sock:")<<s<<" "<<err;}

inline int sockRead (sockType s,char* data,int len
                     ,sockaddr* addr=nullptr,socklen_t* fromLen=nullptr){
  int rc=::recvfrom(s,data,len,0,addr,fromLen);
  if(rc>0)return rc;
  if(rc==0)throw connectionLost("sockRead eof sock:")<<s<<" len:"<<len;
  auto err=GetError();
  if(ECONNRESET==err)throw connectionLost("sockRead-ECONNRESET");
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockRead sock:")<<s<<" len:"<<len<<" "<<err;
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
  if(rc==0)throw connectionLost("Read--eof len: ")<<len;
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  throw failure("Read -- len:")<<len<<" "<<errno;
}
#endif

class SendBuffer{
  int32_t savedSize=0;
protected:
  int index=0;
  int bufsize;
  unsigned char* buf;

public:
  sockType sock_=-1;

  inline SendBuffer (unsigned char* addr,int sz):bufsize(sz),buf(addr){}

  inline void Receive (void const* data,int size){
    if(size>bufsize-index)throw failure("SendBuffer low-level receive:")<<
        size<<" "<<index;
    ::memcpy(buf+index,data,size);
    index+=size;
  }

  template<class T>
  void Receive (T val){
    static_assert(::std::is_arithmetic<T>::value);
    Receive(&val,sizeof(T));
  }

  inline int ReserveBytes (int num){
    if(num>bufsize-index)throw failure("SendBuffer::ReserveBytes");
    auto copy=index;
    index+=num;
    return copy;
  }

  template<class T>
  void Receive (int where,T val){
    static_assert(::std::is_arithmetic<T>::value);
    ::memcpy(buf+where,&val,sizeof(T));
  }

  inline void FillInSize (int32_t max){
    int32_t marshalledBytes=index-savedSize;
    if(marshalledBytes>max)
      throw failure("Size of marshalled data exceeds max: ")<<max;

    Receive(savedSize,marshalledBytes);
    savedSize=index;
  }

  inline void Reset (){savedSize=index=0;}
  inline void Rollback (){index=savedSize;}

  template<typename... T>
  void Receive_variadic (char const* format,T... t){
    auto maxsize=bufsize-index;
    auto size=::snprintf(buf+index,maxsize,format,t...);
    if(size>maxsize)throw failure("SendBuffer::Receive_variadic");
    index+=size;
  }

  inline void ReceiveFile (fileType d,int32_t sz){
    Receive(sz);
    if(sz>bufsize-index)throw failure("SendBuffer ReceiveFile ")<<sz;

    if(Read(d,buf+index,sz)!=sz)throw failure("SendBuffer ReceiveFile Read");
    index+=sz;
  }

  inline bool Flush (::sockaddr* toAddr=nullptr,::socklen_t toLen=0){
    int const bytes=sockWrite(sock_,buf,index,toAddr,toLen);
    if(bytes==index){
      Reset();return true;
    }

    index-=bytes;
    savedSize=index;
    ::memmove(buf,buf+bytes,index);
    return false;
  }

  //UDP-friendly alternative to Flush
  inline void Send (::sockaddr* toAddr=nullptr,::socklen_t toLen=0)
  {sockWrite(sock_,buf,index,toAddr,toLen);}
  inline auto data (){return buf;}
  inline int GetIndex (){return index;}

private:
  SendBuffer (SendBuffer const&);
  SendBuffer& operator= (SendBuffer);
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

inline void Receive (SendBuffer& b,::std::string_view const& s){
  marshallingInt(s.size()).Marshal(b);
  b.Receive(s.data(),s.size());
}

using stringPlus=::std::initializer_list<::std::string_view>;
inline void Receive (SendBuffer& b,stringPlus lst){
  int32_t t=0;
  for(auto s:lst)t+=s.length();
  marshallingInt{t}.Marshal(b);
  for(auto s:lst)b.Receive(s.data(),s.length());//Use low-level Receive
}

inline void InsertNull (SendBuffer& b){uint8_t z=0;b.Receive(z);}

template<class T>
void ReceiveBlock (SendBuffer& b,T const& grp){
  int32_t count=grp.size();
  b.Receive(count);
  if(count>0)
    b.Receive(&*grp.begin(),count*sizeof(typename T::value_type));
}

template<class T>
void ReceiveGroup (SendBuffer& b,T const& grp,bool sendType=false){
  b.Receive(static_cast<int32_t>(grp.size()));
  for(auto const& e:grp)e.Marshal(b,sendType);
}

template<class T>
void ReceiveGroupPointer (SendBuffer& b,T const& grp,bool sendType=false){
  b.Receive(static_cast<int32_t>(grp.size()));
  for(auto p:grp)p->Marshal(b,sendType);
}

//Encode integer into variable-length format.
void marshallingInt::Marshal (SendBuffer& b,bool)const{
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
template<unsigned long N=udp_packet_max>
class SendBufferStack:public SendBuffer{
  unsigned char ar[N];

public:
  inline SendBufferStack ():SendBuffer(ar,N){}
};

class SendBufferHeap:public SendBuffer{
public:
  inline SendBufferHeap (int sz):SendBuffer(new unsigned char[sz],sz){}
  inline ~SendBufferHeap (){delete[] SendBuffer::buf;}
};

template<typename T>
struct Wrapper{
  T* p;
  Wrapper ():p(new T()){}
  ~Wrapper (){delete p;}
  void reset (){::memset(p,0,sizeof(T));}
};

class SendBufferCompressed:public SendBufferHeap{
  Wrapper<::qlz_state_compress> compress;
  int compSize;
  int compIndex=0;
  char* compressedBuf;

  inline bool FlushFlush (::sockaddr* toAddr,::socklen_t toLen){
    int const bytes=sockWrite(sock_,compressedBuf,compIndex,toAddr,toLen);
    if(bytes==compIndex){compIndex=0;return true;}

    compIndex-=bytes;
    ::memmove(compressedBuf,compressedBuf+bytes,compIndex);
    return false;
  }

public:
  inline SendBufferCompressed (int sz):SendBufferHeap(sz),compSize(sz+(sz>>3)+400)
                               ,compressedBuf(new char[compSize]){}

  inline ~SendBufferCompressed (){delete[] compressedBuf;}

  inline bool Flush (::sockaddr* toAddr=nullptr,::socklen_t toLen=0){
    bool rc=true;
    if(compIndex>0)rc=FlushFlush(toAddr,toLen);

    if(index>0){
      if(index+(index>>3)+400>compSize-compIndex)
        throw failure("Not enough room in compressed buf");
      compIndex+=::qlz_compress(buf,compressedBuf+compIndex
                                ,index,compress.p);
      Reset();
      if(rc)rc=FlushFlush(toAddr,toLen);
    }
    return rc;
  }

  inline void CompressedReset (){Reset();compIndex=0;compress.reset();}
};


class SameFormat{
public:
  template<template<class> class B,class U>
  void Read (B<SameFormat>& buf,U& data)
  {buf.Give(&data,sizeof(U));}

  template<template<class> class B,class U>
  void ReadBlock (B<SameFormat>& buf,U* data,int elements)
  {buf.Give(data,elements*sizeof(U));}
};

class LeastSignificantFirst{
public:
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
  void Read (B<LeastSignificantFirst>& buf,float& val){
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
  }

  template<template<class> class B>
  void Read (B<LeastSignificantFirst>& buf,double& val){
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
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

class MostSignificantFirst{
public:
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
  void Read (B<MostSignificantFirst>& buf,float& val){
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
  }

  template<template<class> class B>
  void Read (B<MostSignificantFirst>& buf,double& val){
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
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
  int index=0;
  int packetLength;
  char* const buf;

public:
  explicit ReceiveBuffer (char* addr,int bytes):packetLength(bytes),buf(addr){}

  void Give (void* address,int len){
    if(len>msgLength-index)
      throw failure("ReceiveBuffer::Give: bytes remaining < len ")<<len;
    ::memcpy(address,buf+subTotal+index,len);
    index+=len;
  }

  char GiveOne (){
    if(index>=msgLength)
      throw failure("ReceiveBuffer::GiveOne: out of data");
    return buf[subTotal+index++];
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
      index=0;
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
    while(sz>0){
      int rc=Write(d,buf+subTotal+index,sz);
      sz-=rc;
      index+=rc;
    }
  }

  bool GiveBool (){return GiveOne()!=0;}

  auto GiveString (){
    marshallingInt len(*this);
    if(len()>msgLength-index)throw failure("ReceiveBuffer::GiveString");
    ::std::string s(buf+subTotal+index,len());
    index+=len();
    return s;
  }

  auto GiveString_view (){
    marshallingInt len(*this);
    if(len()>msgLength-index)throw failure("ReceiveBuffer::GiveString_view");
    ::std::string_view v(buf+subTotal+index,len());
    index+=len();
    return v;
  }

  auto GiveString_view_plus (){
    auto v=GiveString_view();
    GiveOne();
    return v;
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

  void AppendTo (::std::string& s){
    marshallingInt len(*this);
    if(len()>msgLength-index)throw failure("ReceiveBuffer::AppendTo");
    s.append(buf+subTotal+index,len());
    index+=len();
  }

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


template<class R,int size=udp_packet_max>
class ReceiveBufferStack:public ReceiveBuffer<R>{
  char ar[size];

public:
  ReceiveBufferStack (sockType s,::sockaddr* fromAddr=nullptr
                      ,::socklen_t* fromLen=nullptr):
    ReceiveBuffer<R>(ar,sockRead(s,ar,size,fromAddr,fromLen))
  {this->NextMessage();}
};


template<class R>
class ReceiveBufferCompressed:Wrapper<::qlz_state_decompress>,public ReceiveBuffer<R>
{
  int const bufsize;
  int bytesRead=0;
  int compressedSize;
  char* compressedStart;

public:
  sockType sock_;

  explicit ReceiveBufferCompressed (int size):ReceiveBuffer<R>(new char[size],0)
					      ,bufsize(size){}

  ~ReceiveBufferCompressed (){delete[] this->buf;}

  bool GotPacket (){
    try{
      if(bytesRead<9){
        bytesRead+=sockRead(sock_,this->buf+bytesRead,9-bytesRead);
        if(bytesRead<9)return false;

        if((this->packetLength=::qlz_size_decompressed(this->buf))>bufsize)
          throw failure("Decompress::GotPacket decompressed size too big ")
                         <<this->packetLength<<" "<<bufsize;

        if((compressedSize=::qlz_size_compressed(this->buf))>bufsize)
          throw failure("Decompress::GotPacket compressed size too big");

        compressedStart=this->buf+bufsize-compressedSize;
        ::memmove(compressedStart,this->buf,9);
      }
      bytesRead+=sockRead(sock_,compressedStart+bytesRead
                          ,compressedSize-bytesRead);
      if(bytesRead<compressedSize)return false;

      ::qlz_decompress(compressedStart,this->buf,Wrapper<::qlz_state_decompress>::p);
      bytesRead=0;
      this->Update();
      return true;
    }catch(...){
      bytesRead=0;
      throw;
    }
  }

  void Reset (){bytesRead=0;Wrapper<::qlz_state_decompress>::reset();}
};

template<int N>
class fixedString{
  marshallingInt len;
  ::std::array<char,N> str;

 public:
  fixedString ()=default;

  inline explicit fixedString (char const* s):len(::strlen(s)){
    if(len()>N-1)throw failure("fixedString ctor");
    ::strcpy(&str[0],s);
  }

  inline explicit fixedString (::std::string_view s):len(s.length()){
    if(len()>N-1)throw failure("fixedString ctor");
    ::strncpy(&str[0],s.data(),len());
    str[len()]='\0';
  }

  template<class R>
  explicit fixedString (ReceiveBuffer<R>& b):len(b){
    if(len()>N-1)throw failure("fixedString stream ctor");
    b.Give(&str[0],len());
    str[len()]='\0';
  }

  inline void Marshal (SendBuffer& b,bool=false)const{
    len.Marshal(b);
    b.Receive(&str[0],len());
  }

  inline auto bytesAvailable (){return N-(len()+1);}

  inline char* operator() (){return &str[0];}
  inline char const* c_str ()const{return &str[0];}
  inline char operator[] (int i)const{return str[i];}
};

using fixedString60=fixedString<60>;
using fixedString120=fixedString<120>;

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

using message_id_8=uint8_t;
using message_id_16=uint16_t;
