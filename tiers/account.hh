#pragma once
#include<marshalling_integer.hh>
#include<fixed_string.hh>

namespace cmw{
class SendBuffer;
template<class R> class ReceiveBuffer;
}

struct cmwAccount{
  ::cmw::marshalling_integer number;
  ::cmw::fixed_string_60 password;

  cmwAccount (int n,char* p):number(n),password(p){}

  template<class R>
  explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void MarshalMemberData (::cmw::SendBuffer&)const;
  void Marshal (::cmw::SendBuffer& b,bool=false)const{MarshalMemberData(b);}
};
