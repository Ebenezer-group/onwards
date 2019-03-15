struct cmwAccount{
  ::cmw::marshallingInt number;
  ::cmw::fixedString60 password;

  cmwAccount (int n,char* p):number(n),password(p){}
  template<class R> explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void MarshalMembers (::cmw::SendBuffer&)const;
  void Marshal (::cmw::SendBuffer& b)const{MarshalMembers(b);}
};

enum class messageID:uint8_t;
