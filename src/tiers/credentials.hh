struct cmwCredentials{
  ::std::string ambassadorID;
  ::cmw::FixedString60 password;

  cmwCredentials ()=default;
  explicit cmwCredentials (::std::string_view id):ambassadorID(id){
    if(ambassadorID.size()>20)::cmw::raise("ambassadorID is too long");
  }

  template<class R,class Z>explicit cmwCredentials (::cmw::ReceiveBuffer<R,Z>&);

  void marshalMembers (auto&)const;
  void marshal (auto& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t;
