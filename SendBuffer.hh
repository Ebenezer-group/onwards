#pragma once
#include"ErrorWords.hh"
#include"IO.hh"
#include"marshallingInt.hh"
#include"quicklz.h"
#include"udpStuff.hh"
#include<initializer_list>
#include<stdint.h>
#include<stdio.h>//snprintf
#include<string>
#include<string_view>
#include<string.h>//memcpy
#include<type_traits>

#include<limits>
static_assert(::std::numeric_limits<unsigned char>::digits==8
              ,"Only 8 bit char supported");
static_assert(::std::numeric_limits<float>::is_iec559
              ,"Only IEEE 754 supported");
#ifndef CMW_WINDOWS
#include<sys/types.h>
#include<sys/socket.h>
#endif

namespace cmw{
using stringPlus=::std::initializer_list<::std::string_view>;

class SendBuffer{
  int32_t savedSize=0;

protected:
  int index=0;
  int bufsize;
  unsigned char* buf;

public:
  sock_type sock_=-1;

  inline SendBuffer (unsigned char* addr,int sz):bufsize(sz),buf(addr){}

  auto data (){return buf;}

  inline void Receive (void const* data,int size){
    if(size>bufsize-index)
      throw failure("Size of marshalled data exceeds space available: ")
        <<bufsize<<" "<<savedSize;
    ::memcpy(buf+index,data,size);
    index+=size;
  }

  template<typename... T>
  void Receive_variadic (char const* format,T... t){
    auto maxsize=bufsize-index;
    auto size=::snprintf(buf+index,maxsize,format,t...);
    if(size>maxsize)throw failure("SendBuffer::Receive_variadic");
    index+=size;
  }

  template<class T>
  void Receive (T val){
    static_assert(::std::is_arithmetic<T>::value
                  ,"SendBuffer expecting arithmetic type");
    Receive(&val,sizeof(T));
  }

  template<class T>
  void Receive (int where,T val){
    static_assert(::std::is_arithmetic<T>::value
                  ,"SendBuffer expecting arithmetic type");
    ::memcpy(buf+where,&val,sizeof(T));
  }

  inline void ReceiveFile (file_type d,int32_t sz){
    Receive(sz);
    if(sz>bufsize-index)
      throw failure("SendBuffer::ReceiveFile ")<<bufsize;

    if(Read(d,&buf[index],sz)!=sz)
      throw failure("SendBuffer::ReceiveFile Read");
    index+=sz;
  }

  inline void Receive (bool b){Receive(static_cast<unsigned char>(b));}

  inline void Receive (char* cstr){
    marshallingInt len(::strlen(cstr));
    len.Marshal(*this);
    Receive(cstr,len());
  }

  inline void Receive (char const* cstr){
    marshallingInt len(::strlen(cstr));
    len.Marshal(*this);
    Receive(cstr,len());
  }

  inline void Receive (::std::string const& s){
    marshallingInt len(s.size());
    len.Marshal(*this);
    Receive(s.data(),len());
  }

  inline void Receive (::std::string_view const& s){
    marshallingInt len(s.size());
    len.Marshal(*this);
    Receive(s.data(),len());
  }

  inline void Receive (stringPlus lst){
    int32_t t=0;
    for(auto s:lst)t+=s.length();
    marshallingInt{t}.Marshal(*this);
    for(auto s:lst)Receive(s.data(),s.length());//Use low-level Receive
  }

  inline void InsertNull (){uint8_t z=0;Receive(z);}
  inline int GetIndex (){return index;}
  inline int ReserveBytes (int num){
    if(num>bufsize-index)throw failure("SendBuffer::ReserveBytes");
    auto copy=index;
    index+=num;
    return copy;
  }

  inline void FillInSize (int32_t max){
    int32_t marshalledBytes=index-savedSize;
    if(marshalledBytes>max)
      throw failure("Size of marshalled data exceeds max of: ")<<max;

    Receive(savedSize,marshalledBytes);
    savedSize=index;
  }

  inline void Reset (){savedSize=index=0;}
  inline void Rollback (){index=savedSize;}

  template<class T>
  void ReceiveBlock (T const& grp){
    int32_t count=grp.size();
    Receive(count);
    if(count>0)
      Receive(&*grp.begin(),count*sizeof(typename T::value_type));
  }

  template<class T>
  void ReceiveGroup (T const& grp,bool sendType=false){
    Receive(static_cast<int32_t>(grp.size()));
    for(auto const& e:grp)e.Marshal(*this,sendType);
  }

  template<class T>
  void ReceiveGroupPointer (T const& grp,bool sendType=false){
    Receive(static_cast<int32_t>(grp.size()));
    for(auto p:grp)p->Marshal(*this,sendType);
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

private:
  SendBuffer (SendBuffer const&);
  SendBuffer& operator= (SendBuffer);
};

template<unsigned long N=udp_packet_max>
class SendBufferStack:public SendBuffer{
  unsigned char ar[N];

public:
  SendBufferStack ():SendBuffer(ar,N){}
};

class SendBufferHeap:public SendBuffer{
public:
  inline SendBufferHeap (int sz):SendBuffer(new unsigned char[sz],sz){}
  inline ~SendBufferHeap (){delete [] SendBuffer::buf;}
};

struct compress_wrapper{
  ::qlz_state_compress* qlz_compress;
  compress_wrapper ():qlz_compress(new ::qlz_state_compress()){}
  ~compress_wrapper (){delete qlz_compress;}
};

class SendBufferCompressed:public SendBufferHeap{
  compress_wrapper compress;
  int compSize;
  int compIndex=0;
  char* compressedBuf;

  bool FlushFlush (::sockaddr* toAddr,::socklen_t toLen){
    int const bytes=sockWrite(sock_,compressedBuf,compIndex,toAddr,toLen);
    if(bytes==compIndex){compIndex=0;return true;}

    compIndex-=bytes;
    ::memmove(compressedBuf,compressedBuf+bytes,compIndex);
    return false;
  }

public:
  SendBufferCompressed (int sz):SendBufferHeap(sz),compSize(sz+(sz>>3)+400)
                                ,compressedBuf(new char[compSize]){}

  ~SendBufferCompressed (){delete[] compressedBuf;}

  bool Flush (::sockaddr* toAddr=nullptr,::socklen_t toLen=0){
    bool rc=true;
    if(compIndex>0)rc=FlushFlush(toAddr,toLen);

    if(index>0){
      if(index+(index>>3)+400>compSize-compIndex)
        throw failure("Not enough room in compressed buf");
      compIndex+=::qlz_compress(buf,compressedBuf+compIndex
                                ,index,compress.qlz_compress);
      Reset();
      if(rc)rc=FlushFlush(toAddr,toLen);
    }
    return rc;
  }

  inline void CompressedReset (){Reset();compIndex=0;
    ::memset(compress.qlz_compress,0,sizeof(::qlz_state_compress));
  }
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

  void GiveFile (file_type d){
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
  ReceiveBufferStack (sock_type sock,::sockaddr* fromAddr=nullptr
                      ,::socklen_t* fromLen=nullptr):
    ReceiveBuffer<R>(ar,sockRead(sock,ar,size,fromAddr,fromLen))
  {this->NextMessage();}
};

struct decompress_wrapper{
  ::qlz_state_decompress* qlz_decompress;
  decompress_wrapper ():qlz_decompress(new ::qlz_state_decompress()){}
  ~decompress_wrapper (){delete qlz_decompress;}
};

template<class R>
class ReceiveBufferCompressed:private decompress_wrapper,public ReceiveBuffer<R>
{
  int const bufsize;
  int bytesRead=0;
  int compressedSize;
  char* compressedStart;

public:
  sock_type sock_;

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

      ::qlz_decompress(compressedStart,this->buf,decompress_wrapper::qlz_decompress);
      bytesRead=0;
      this->Update();
      return true;
    }catch(...){
      bytesRead=0;
      throw;
    }
  }

  void Reset (){bytesRead=0;
    ::memset(decompress_wrapper::qlz_decompress,0,sizeof(::qlz_state_decompress));
  }
};
}
