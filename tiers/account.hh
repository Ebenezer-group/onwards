#pragma once
#include<marshalling_integer.hh>
#include<fixedString.hh>

namespace cmw{
class SendBuffer;
template<class R> class ReceiveBuffer;
}

struct cmwAccount{
  ::cmw::marshalling_integer number;
  ::cmw::fixedString_60 password;

  cmwAccount (int n,char* p):number(n),password(p){}

  template<class R>
  explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void MarshalMemberData (::cmw::SendBuffer&)const;
  void Marshal (::cmw::SendBuffer& b,bool=false)const{MarshalMemberData(b);}
};
