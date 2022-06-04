struct cmwAccount{
  ::cmw::MarshallingInt number;
#ifdef CMW_MIDDLE
  ::cmw::FixedString60 password;
#elifdef CMW_BACK
  ::std::string_view  password;
#endif

  cmwAccount (int n,::std::string_view p):number(n),password(p){}
  template<class R>explicit cmwAccount (::cmw::ReceiveBuffer<R>&);

  void marshalMembers (::cmw::SendBuffer&)const;
  void marshal (::cmw::SendBuffer& b)const{marshalMembers(b);}
};

enum class messageID:uint8_t;
