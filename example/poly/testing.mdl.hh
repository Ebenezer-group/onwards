//File generated by the C++ Middleware Writer version 1.15.
#ifndef testing_mdl_hh
#define testing_mdl_hh
inline void
base::marshalMembers (auto& buf)const{
}

template<class R,class Z>
base::base (::cmw::ReceiveBuffer<R,Z>& buf)
{}

void baseSwitch (auto& c,auto& buf){
  switch(auto typeNum=::cmw::give<::uint8_t>(buf);typeNum){
    case base::typeNum:
      ::cmw::buildSegment<base>(c,buf);break;
    case derived1::typeNum:
      ::cmw::buildSegment<derived1>(c,buf);break;
    case derived3::typeNum:
      ::cmw::buildSegment<derived3>(c,buf);break;
    case derived2::typeNum:
      ::cmw::buildSegment<derived2>(c,buf);break;
    default: ::cmw::raise("baseSwitch: Unknown type");
  }
}

inline void
derived1::marshalMembers (auto& buf)const{
  base::marshalMembers(buf);
  buf.receive(a);
}

template<class R,class Z>
derived1::derived1 (::cmw::ReceiveBuffer<R,Z>& buf):
   base(buf),a(::cmw::give<uint32_t>(buf))
{}

inline void
derived2::marshalMembers (auto& buf)const{
  base::marshalMembers(buf);
  buf.receive(b);
}

template<class R,class Z>
derived2::derived2 (::cmw::ReceiveBuffer<R,Z>& buf):
   base(buf),b(::cmw::give<uint32_t>(buf))
{}

inline void
derived3::marshalMembers (auto& buf)const{
  derived1::marshalMembers(buf);
  buf.receive(c);
}

template<class R,class Z>
derived3::derived3 (::cmw::ReceiveBuffer<R,Z>& buf):
   derived1(buf),c(::cmw::give<double>(buf))
{}

struct testing{
static void marshal (auto& buf
         ,::boost::base_collection<base> const& a)try{
  buf.reserveBytes();
  ::cmw::marshalCollection<base,derived1,derived3,derived2>(a,buf);
  buf.fillInSize(10000);
}catch(...){buf.rollback();throw;}

static void give (auto& buf
         ,::boost::base_collection<base>& a){
  ::cmw::buildCollection(a,buf,[](auto& a,auto& buf){baseSwitch(a,buf);});
}
};
#endif
