struct cmwAccount{
  ::cmw::MarshallingInt number;
  ::std::string_view password;

  cmwAccount (){}
  cmwAccount (int n,char const *p):number(n),password(p){}
  template<class R>explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t;
