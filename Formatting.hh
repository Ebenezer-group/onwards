#pragma once

#include<stdint.h>
#include<string.h> //memcpy

class SameFormat
{
public:
  template <template<class> class B,class U>
  void Read (B<SameFormat>& buf,U& data)
  {
    buf.Give(&data,sizeof(U));
  }

  template <template<class> class B,class U>
  void ReadBlock (B<SameFormat>& buf
                  ,U* data,int elements)
  {
    buf.Give(data,elements*sizeof(U));
  }
};


class LeastSignificantFirst
{
public:
  template <template<class> class B>
  void Read (B<LeastSignificantFirst>& buf
             ,uint8_t& val)
  {
    val=buf.GiveOne();
  }

  template <template<class> class B>
  void Read (B<LeastSignificantFirst>& buf
             ,uint16_t& val)
  {
    val=buf.GiveOne();
    val|=buf.GiveOne()<<8;
  }

  template <template<class> class B>
  void Read (B<LeastSignificantFirst>& buf
             ,uint32_t& val)
  {
    val=buf.GiveOne();
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<24;
  }

  template <template<class> class B>
  void Read (B<LeastSignificantFirst>& buf
             ,uint64_t& val)
  {
    val=buf.GiveOne();
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<24;
    val|=(uint64_t)buf.GiveOne()<<32;
    val|=(uint64_t)buf.GiveOne()<<40;
    val|=(uint64_t)buf.GiveOne()<<48;
    val|=(uint64_t)buf.GiveOne()<<56;
  }

  template <template<class> class B>
  void Read (B<LeastSignificantFirst>& buf
             ,float& val)
  {
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
  }

  template <template<class> class B>
  void Read (B<LeastSignificantFirst>& buf
             ,double& val)
  {
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
  }

  template <template<class> class B,class U>
  void ReadBlock (B<LeastSignificantFirst>& buf
                  ,U* data,int elements)
  {
    for(int i=0;i<elements;++i){
      *(data+i)=buf.template Give<U>();
    }
  }

  // Overloads for uint8_t and int8_t
  template <template<class> class B>
  void ReadBlock (B<LeastSignificantFirst>& buf
                  ,uint8_t* data,int elements)
  {
    buf.Give(data,elements);
  }

  template <template<class> class B>
  void ReadBlock (B<LeastSignificantFirst>& buf
                  ,int8_t* data,int elements)
  {
    buf.Give(data,elements);
  }
};


class MostSignificantFirst
{
public:
  template <template<class> class B>
  void Read (B<MostSignificantFirst>& buf
             ,uint8_t& val)
  {
    val=buf.GiveOne();
  }

  template <template<class> class B>
  void Read (B<MostSignificantFirst>& buf
             ,uint16_t& val)
  {
    val=buf.GiveOne()<<8;
    val|=buf.GiveOne();
  }

  template <template<class> class B>
  void Read (B<MostSignificantFirst>& buf
             ,uint32_t& val)
  {
    val=buf.GiveOne()<<24;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne();
  }

  template <template<class> class B>
  void Read (B<MostSignificantFirst>& buf
             ,uint64_t& val)
  {
    val=(uint64_t)buf.GiveOne()<<56;
    val|=(uint64_t)buf.GiveOne()<<48;
    val|=(uint64_t)buf.GiveOne()<<40;
    val|=(uint64_t)buf.GiveOne()<<32;
    val|=buf.GiveOne()<<24;
    val|=buf.GiveOne()<<16;
    val|=buf.GiveOne()<<8;
    val|=buf.GiveOne();
  }

  template <template<class> class B>
  void Read (B<MostSignificantFirst>& buf
             ,float& val)
  {
    uint32_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
  }

  template <template<class> class B>
  void Read (B<MostSignificantFirst>& buf
             ,double& val)
  {
    uint64_t tmp;
    this->Read(buf,tmp);
    ::memcpy(&val,&tmp,sizeof(val));
  }

  template <template<class> class B,class U>
  void ReadBlock (B<MostSignificantFirst>& buf
                  ,U* data,int elements)
  {
    for(int i=0;i<elements;++i){
      *(data+i)=buf.template Give<U>();
    }
  }

  template <template<class> class B>
  void ReadBlock (B<MostSignificantFirst>& buf
                  ,uint8_t* data,int elements)
  {
    buf.Give(data,elements);
  }

  template <template<class> class B>
  void ReadBlock (B<MostSignificantFirst>& buf
                  ,int8_t* data,int elements)
  {
    buf.Give(data,elements);
  }
};

uint8_t const least_significant_first=0;
uint8_t const most_significant_first=1;

