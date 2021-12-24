//File generated by the C++ Middleware Writer version 1.14.
#ifndef example_mdl_hh
#define example_mdl_hh
struct exampleMessages{
static ::int32_t mar (auto& buf
         ,::std::vector<::int32_t> const& a
         ,::std::string const& b){
  receiveBlock(buf,a);
  receive(buf,b);
  return 10000;
}

static void give (auto& buf
         ,::std::vector<::int32_t>& a
         ,::std::string& b){
  if(int32_t ca=::cmw::give<uint32_t>(buf);ca>0){
    a.resize(a.size()+ca);
    buf.giveBlock(&*(a.end()-ca),ca);
  }
  b=buf.giveStringView();
}

static ::int32_t mar (auto& buf
         ,::std::set<::int32_t> const& a){
  buf.receive<int32_t>(a.size());
  for(auto const& e1:a){
    buf.receive(e1);
  }
  return 10000;
}

static void give (auto& buf
         ,::std::set<::int32_t>& a){
  auto z3=a.end();
  for(int32_t ca=::cmw::give<uint32_t>(buf);ca>0;--ca){
    a.emplace_hint(z3,::cmw::give<uint32_t>(buf));
  }
}

static ::int32_t mar (auto& buf
         ,::std::array<float,6> const& a){
  buf.receive(&a,sizeof a);
  return 10000;
}

static void give (auto& buf
         ,::std::array<float,6>& a){
  buf.giveBlock(&a[0],sizeof a/sizeof(float));
}

template<messageID id,class...T>
static void marshal (::cmw::SendBuffer& buf,T&&...t)try{
  buf.reserveBytes(4);
  buf.receive(id);
  buf.fillInSize(mar(buf,::std::forward<T>(t)...));
}catch(...){buf.rollback();throw;}
};
#endif
