#pragma once
#include"ErrorWords.hh"
#include"IO.hh"
#include"marshalling_integer.hh"
#include<stdint.h>
#include<string>
#include<string_view>
#include<string.h> //memcpy

#include<limits>
static_assert(::std::numeric_limits<unsigned char>::digits==8
              ,"Only 8 bit char supported");
static_assert(::std::numeric_limits<float>::is_iec559
              ,"Only IEEE 754 supported");

namespace cmw{

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
    for(int i=0;i<elements;++i){
      *(data+i)=buf.template Give<U>();
    }
  }

  // Overloads for uint8_t and int8_t
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
    for(int i=0;i<elements;++i){
      *(data+i)=buf.template Give<U>();
    }
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

  void GiveFile (file_type fd){
    int sz=Give<uint32_t>();
    while(sz>0){
      int rc=Write(fd,buf+subTotal+index,sz);
      sz-=rc;
      index+=rc;
    }
  }

  bool GiveBool (){return GiveOne()!=0;}

  auto GiveString (){
    marshalling_integer len(*this);
    if(len()>msgLength-index)
      throw failure("ReceiveBuffer::GiveString");
    ::std::string s(buf+subTotal+index,len());
    index+=len();
    return s;
  }

  auto GiveString_view(){
    marshalling_integer len(*this);
    if(len()>msgLength-index)
      throw failure("ReceiveBuffer::GiveString_view");
    ::std::string_view v(buf+subTotal+index,len());
    index+=len();
    return v;
  }

  auto GiveString_view_plus(){
    auto v=GiveString_view();
    GiveOne();
    return v;
  }

#ifndef CMW_WINDOWS
  template<ssize_t N>
  void CopyString (char (&dest)[N]){
    marshalling_integer len(*this);
    if(len()+1>N)throw failure("ReceiveBuffer::CopyString");
    Give(dest,len());
    dest[len()]='\0';
  }
#endif

  void AppendTo(::std::string& s){
    marshalling_integer len(*this);
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
    auto endIt(rbt.end());
    for(int count=Give<uint32_t>();count>0;--count){
      rbt.insert_unique(endIt,*T::value_type::BuildPolyInstance(*this));
    }
  }

private:
  ReceiveBuffer (ReceiveBuffer const&);
  ReceiveBuffer& operator= (ReceiveBuffer);
};

template<class T,class R>
T Give (ReceiveBuffer<R>& buf){return buf.template Give<T>();}
}
