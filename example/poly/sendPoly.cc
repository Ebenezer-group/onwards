#include<base.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"testing.hh"

int main (){
  using namespace ::cmw;
  ::boost::base_collection<base> c;

  derived1 d1;
  derived3 d3;
  c.insert(d1);
  c.insert(d3);

  GetaddrinfoWrapper res("127.0.0.1" ,"13579",SOCK_DGRAM); //may need ::1
  BufferStack<SameFormat> buf(res.getSock());
  testing::marshal(buf,c);
  buf.send(res()->ai_addr,res()->ai_addrlen);
}
