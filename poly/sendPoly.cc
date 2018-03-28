#include<boost/poly_collection/base_collection.hpp>
#include<iostream>
#include<testing.hh>
#include<zz.testing.hh>
#include<wrappers.hh>

int main (){
  using namespace ::cmw;
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
  BufferStack<SameFormat> buf;
  buf.sock_=res.getSock();
  testing::Marshal(buf,c);
  buf.Send(res()->ai_addr,res()->ai_addrlen);
}
