#pragma once
#include<fixedString.hh>
#include<marshallingInt.hh>

namespace cmw{
class SendBuffer;
template<class R> class ReceiveBuffer;
}

struct cmwAccount{
  ::cmw::marshallingInt number;
  ::cmw::fixedString60 password;

  cmwAccount (int n,char* p):number(n),password(p){}

  template<class R>
  explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void MarshalMemberData (::cmw::SendBuffer&)const;
  void Marshal (::cmw::SendBuffer& b,bool=false)const{MarshalMemberData(b);}
};
