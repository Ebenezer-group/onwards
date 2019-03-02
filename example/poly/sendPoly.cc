#include<testing.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"zz.testing.hh"
using namespace ::cmw;

int main (){
  ::boost::base_collection<base> c;

  derived1 d1;
  derived3 d3;
  c.insert(d1);
  c.insert(d3);

  getaddrinfoWrapper res(
#ifdef __linux__
                         "127.0.0.1"
#else
                         "::1"
#endif
                         ,"13579",SOCK_DGRAM);
  BufferStack<SameFormat> buf(res.getSock());
  testing::Marshal(buf,c);
  buf.Send(res()->ai_addr,res()->ai_addrlen);
}
