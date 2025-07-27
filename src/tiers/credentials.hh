struct Credentials{
  ::std::string ambassID;
  ::cmw::FixedString60 password;

  explicit Credentials (::std::string_view id):ambassID(id){
    if(ambassID.size()>20)::cmw::raise("ambassID length");
  }

  template<class R,class Z>explicit Credentials (::cmw::ReceiveBuffer<R,Z>&);

  void marshalMembers (auto&)const;
  void marshal (auto& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t{signup,login,generate};
