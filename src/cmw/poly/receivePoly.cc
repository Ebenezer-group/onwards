#include <Buffer.hh>
#include <wrappers.hh>
#include "zz.testing.hh"
#include <iostream>

using namespace ::cmw;

int main()
{
  try{
    BufferStack<SameFormat> buffer;
    buffer.sock_=udpServer("13579");

    for(;;){
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
