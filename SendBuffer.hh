#pragma once
#include"ErrorWords.hh"
#include"IO.hh"
#include"marshalling_integer.hh"
#include"platforms.hh"
#include"udp_stuff.hh"
#include<initializer_list>
#include<stdint.h>
#include<stdio.h>//snprintf
#include<string>
#include<string_view>
#include<string.h>
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

  inline void ReceiveFile (file_type fd,int32_t fl_sz){
    Receive(fl_sz);
    if(fl_sz> bufsize-index)
      throw failure("SendBuffer::ReceiveFile ")<<bufsize;

    if(Read(fd,&buf[index],fl_sz)!=fl_sz)
      throw failure("SendBuffer::ReceiveFile Read");
    index+=fl_sz;
  }

  inline void Receive (bool b){Receive(static_cast<unsigned char>(b));}

  inline void Receive (char* cstr){
    marshalling_integer len(::strlen(cstr));
    len.Marshal(*this);
    Receive(cstr,len());
  }

  inline void Receive (char const* cstr){
    marshalling_integer len(::strlen(cstr));
    len.Marshal(*this);
    Receive(cstr,len());
  }

  inline void Receive (::std::string const& s){
    marshalling_integer len(s.size());
    len.Marshal(*this);
    Receive(s.data(),len());
  }

  inline void Receive (::std::string_view const& s){
    marshalling_integer len(s.size());
    len.Marshal(*this);
    Receive(s.data(),len());
  }

  inline void Receive (stringPlus lst){
    int32_t t=0;
    for(auto s:lst)t+=s.length();
    marshalling_integer{t}.Marshal(*this);
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
    for(auto const& it:grp)it.Marshal(*this,sendType);
  }

  template<class T>
  void ReceiveGroupPointer (T const& grp,bool sendType=false){
    Receive(static_cast<int32_t>(grp.size()));
    for(auto const& it:grp)it->Marshal(*this,sendType);
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
}
