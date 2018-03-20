#include<boost/poly_collection/base_collection.hpp>
#include<iostream>
#include<testing.hh>
#include<zz.testing.hh>
#include<wrappers.hh>

int main (){
  ::boost::base_collection<base> c;
  using namespace ::cmw;

  derived1 d1;
  derived3 d3;
  c.insert(d1);
  c.insert(d3);

  BufferStack<SameFormat> buf;
  getaddrinfoWrapper res(
#ifdef __linux__
                         "127.0.0.1"
#else
		         "::1"
#endif
		         ,"12345",SOCK_DGRAM);
  buf.sock_=res.getSock();
  testing::Marshal(buf,c);
  buf.Send(res()->ai_addr,res()->ai_addrlen);
}
