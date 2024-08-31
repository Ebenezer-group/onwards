//File generated by the C++ Middleware Writer version 1.15.
#pragma once

inline void
cmwCredentials::marshalMembers (auto& buf)const{
  receive(buf,ambassID);
  password.marshal(buf);
}

struct back{
static void mar (auto& buf
         ,cmwCredentials const& a){
  a.marshal(buf);
}

static void mar (auto& buf
         ,cmwCredentials const& a
         ,::int32_t b){
  a.marshal(buf);
  receive(buf,b);
}

static void mar (auto& buf
         ,cmwRequest const& a){
  a.marshal(buf);
}

template<messageID id,int maxLength=10000>
static void marshal (auto& buf,auto&&...t)try{
  buf.reserveBytes(buf.getZ()+sizeof id);
  mar(buf,::std::forward<decltype(t)>(t)...);
  buf.fillInHdr(maxLength,id);
}catch(...){buf.rollback();throw;}
};

struct front{
template<int maxLength=10000>
static void marshal (auto& buf
         ,::cmw::stringPlus const& a={}
         ,::int8_t b={})try{
  buf.reserveBytes();
  receive(buf,a.size()==0);
  if(a.size()>0){
    receive(buf,a);
    receive(buf,b);
  }
  buf.fillInSize(maxLength);
}catch(...){buf.rollback();throw;}
};
