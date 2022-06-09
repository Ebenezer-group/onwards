struct cmwCredentials{
  ::cmw::FixedString16 userID;
  ::cmw::FixedString60 password;

  cmwCredentials()=default;
  template<class R>explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t;
