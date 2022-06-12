struct cmwCredentials{
  ::cmw::FixedString20 userID;
  ::cmw::FixedString60 password;

  cmwCredentials ()=default;
  template<class R>explicit cmwCredentials (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t;
