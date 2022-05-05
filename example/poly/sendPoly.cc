#include<base.hh>
#include<cmw_Buffer.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"testing.mdl.hh"

int main (){
  ::boost::base_collection<base> c;

  derived1 d1;
  derived3 d3;
  c.insert(d1);
  c.insert(d3);

  ::cmw::GetaddrinfoWrapper res("127.0.0.1" ,"13579",SOCK_DGRAM); //may need ::1
  ::cmw::BufferStack<::cmw::SameFormat> buf(res.getSock());
  ::testing::marshal(buf,c);
  buf.send(res().ai_addr,res().ai_addrlen);
}
