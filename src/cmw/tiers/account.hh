struct cmwAccount{
  marshallingInt number;
  fixedString60 password;

  cmwAccount (int n,char* p):number(n),password(p){}
  template<class R> explicit cmwAccount (ReceiveBuffer<R>&);

  void MarshalMembers (SendBuffer&)const;
  void Marshal (SendBuffer& b)const{MarshalMembers(b);}
};

enum class messageID:uint8_t;
