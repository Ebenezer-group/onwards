#pragma once

#include "ErrorWords.hh"
#include "Formatting.hh"
#include "IOfile.hh"
#include "marshalling_integer.hh"
#include <stdint.h>
#include <string>
#include <string_view>
#include <string.h>

#include <limits>
static_assert(::std::numeric_limits<unsigned char>::digits==8
              ,"Only 8 bit char supported");
static_assert(::std::numeric_limits<float>::is_iec559
              ,"Only IEEE 754 supported");

namespace cmw {

template <class R>
class ReceiveBuffer
{
  R reader;
  int msgLength=0;
  int subTotal=0;

protected:
  int index=0;
  int packetLength;
  char* const buf;

public:
  explicit ReceiveBuffer (char* addr,int bytes):packetLength(bytes),buf(addr)
  {}

  void Give (void* address,int len)
  {
    if(len>msgLength-index)
      throw failure("ReceiveBuffer::Give -- bytes remaining < len ")<<len;
    ::memcpy(address,buf+subTotal+index,len);
    index+=len;
  }

  char GiveOne ()
  {
    if(index>=msgLength)
      throw failure("ReceiveBuffer::GiveOne -- out of data");
    return buf[subTotal+index++];
  }

  template <class T>
  T Give ()
  {
    T tmp;
    reader.Read(*this,tmp);
    return tmp;
  }

  bool NextMessage ()
  {
    subTotal+=msgLength;
    if(subTotal<packetLength){
      index=0;
      msgLength=sizeof(int32_t);
      msgLength=Give<uint32_t>();
      return true;
    }
    return false;
  }

  void Update ()
  {
    msgLength=subTotal=0;
    NextMessage();
  }

  template <class T>
  void GiveBlock (T* data,unsigned int elements)
  {reader.ReadBlock(*this,data,elements);}

  void GiveFile (file_type fd)
  {
    int sz=Give<uint32_t>();
    while(sz>0){
      int rc=Write(fd,buf+subTotal+index,sz);
      sz-=rc;
      index+=rc;
    }
  }

  bool GiveBool () {return GiveOne()!=0;}

  auto GiveString ()
  {
    marshalling_integer slen(*this);
    if(slen()>msgLength-index)
      throw failure("ReceiveBuffer::GiveString");
    ::std::string str(buf+subTotal+index,slen());
    index+=slen();
    return str;
  }

  auto GiveString_view()
  {
    marshalling_integer slen(*this);
    if(slen()>msgLength-index)
      throw failure("ReceiveBuffer::GiveString_view");
    ::std::string_view view(buf+subTotal+index,slen());
    index+=slen();
    return view;
  }

  template <ssize_t N>
  void CopyString (char (&dest)[N])
  {
    marshalling_integer slen(*this);
    if(slen()+1>N)throw failure("ReceiveBuffer::CopyString");
    Give(dest,slen());
    dest[slen()]='\0';
  }

  void AppendTo(::std::string& s)
  {
    marshalling_integer slen(*this);
    if(slen()>msgLength-index)throw failure("ReceiveBuffer::AppendTo");
    s.append(buf+subTotal+index,slen());
    index+=slen();
  }

  template <class T>
  void Giveilist (T& intrlst)
  {
    int count=Give<uint32_t>();
    for(;count>0;--count)
      intrlst.push_back(*T::value_type::BuildPolyInstance(*this));
  }

  template <class T>
  void Giverbtree (T& rbt)
  {
    int count=Give<uint32_t>();
    auto endIt(rbt.end());
    for(;count>0;--count){
      rbt.insert_unique(endIt,*T::value_type::BuildPolyInstance(*this));
    }
  }

private:
  ReceiveBuffer (ReceiveBuffer const&);
  ReceiveBuffer& operator= (ReceiveBuffer);
};
}
