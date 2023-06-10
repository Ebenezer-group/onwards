#include<cmwComplex.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"base.hh"
#include"testing.mdl.hh"

int main (){
  ::boost::base_collection<base> c;

  c.insert(derived1{});
  c.insert(derived3{});

  ::cmw::GetaddrinfoWrapper res("127.0.0.1" ,"13579",SOCK_DGRAM); //may need ::1
  ::cmw::BufferStack<::cmw::SameFormat> buf(res.getSock());
  ::testing::marshal(buf,c);
  buf.send(res().ai_addr,res().ai_addrlen);
}
