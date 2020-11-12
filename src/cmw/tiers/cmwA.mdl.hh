//File generated by the C++ Middleware Writer version 1.14.
#ifndef cmwA_mdl_hh
#define cmwA_mdl_hh
inline void
cmwAccount::marshalMembers (::cmw::SendBuffer& buf)const{
  number.marshal(buf);
  receive(buf,password);
}

struct back{
static ::int32_t mar (::cmw::SendBuffer& buf
         ,::std::vector<cmwAccount> const& a
         ,::int32_t b){
  receiveBlock(buf,a);
  buf.receive(b);
  return 10000;
}

static ::int32_t mar (::cmw::SendBuffer& buf
         ,cmwRequest const& a){
  a.marshal(buf);
  return 700000;
}

template<messageID id,class...T>
static void marshal (::cmw::SendBuffer& buf,T&&...t)try{
  buf.reserveBytes(4);
  buf.receive<messageID>(id);
  buf.fillInSize(mar(buf,t...));
}catch(...){buf.rollback();throw;}
};

struct front{
template<bool res>
static void marshal (::cmw::SendBuffer& buf
         ,::cmw::stringPlus const& a={}
         ,::int8_t b={})try{
  buf.reserveBytes(4);
  receiveBool(buf,res);
  if(!res){
    receive(buf,a);
    buf.receive(b);
  }
  buf.fillInSize(udp_packet_max);
}catch(...){buf.rollback();throw;}
};
#endif
