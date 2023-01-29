struct cmwCredentials{
  ::std::string ambassadorID;
  ::cmw::FixedString60 password;

  cmwCredentials ()=default;
  template<class R>explicit cmwCredentials (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (auto&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t;
