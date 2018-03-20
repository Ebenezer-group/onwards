//  This program receives messages sent by sendExample.

#include <Buffer.hh>
#include <wrappers.hh>
#include "zz.testing.hh"

#include <iostream>

using namespace ::cmw;

int main()
{
  try{
    auto sd=udpServer("12345");

    for(;;){
      BufferStack<SameFormat> buffer;
      buffer.sock_=sd;
      buffer.GetPacket();
      ::boost::base_collection<base> b;
      testing::Give(buffer,b);
      ::std::cout << "size is " << b.size()<<::std::endl;
      ::std::cout << "size of der1is " << b.size<derived1>()<<::std::endl;
      ::std::cout << "size of der3is " << b.size<derived3>()<<::std::endl;
    }
    return 1;
  } catch(std::exception const& ex){
    ::std::cout << "failure: " << ex.what()<<::std::endl;
  }
}
