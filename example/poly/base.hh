#ifndef BASE_HH
#define BASE_HH
#include<cmw_Buffer.hh>
#include<cmw_Complex.hh>

struct base{
  static uint8_t const typeNum=0;
  base ()=default;
  virtual ~base(){}

  template<class R>explicit base (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

class derived1:public base{
  int32_t a;
public:
  static uint8_t const typeNum=1;
  derived1 ()=default;

  template<class R>explicit derived1 (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

class derived2:public base{
  int32_t b;
public:
  static uint8_t const typeNum=2;
  derived2 ()=default;

  template<class R>explicit derived2 (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

class derived3:public derived1{
  double c=3.14;
public:
  static uint8_t const typeNum=3;
  derived3 ()=default;

  template<class R>explicit derived3 (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};
#endif
