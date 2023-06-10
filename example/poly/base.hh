#ifndef BASE_HH
#define BASE_HH
#include<cmwBuffer.hh>

struct base{
  static uint8_t const typeNum=0;
  base ()=default;
  virtual ~base(){}

  template<class R,class Z>explicit base (::cmw::ReceiveBuffer<R,Z>&);

  void marshalMembers (auto&)const;
  void marshal (auto& b)const{marshalMembers(b);}
};

class derived1:public base{
  int32_t a;
public:
  static uint8_t const typeNum=1;
  derived1 ()=default;

  template<class R,class Z>explicit derived1 (::cmw::ReceiveBuffer<R,Z>&);

  void marshalMembers (auto&)const;
  void marshal (auto& b)const{marshalMembers(b);}
};

class derived2:public base{
  int32_t b;
public:
  static uint8_t const typeNum=2;
  derived2 ()=default;

  template<class R,class Z>explicit derived2 (::cmw::ReceiveBuffer<R,Z>&);

  void marshalMembers (auto&)const;
  void marshal (auto& b)const{marshalMembers(b);}
};

class derived3:public derived1{
  double c=3.14;
public:
  static uint8_t const typeNum=3;
  derived3 ()=default;

  template<class R,class Z>explicit derived3 (::cmw::ReceiveBuffer<R,Z>&);

  void marshalMembers (auto&)const;
  void marshal (auto& b)const{marshalMembers(b);}
};
#endif
