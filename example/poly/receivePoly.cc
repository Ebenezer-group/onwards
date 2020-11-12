#include<cmw/Buffer.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"base.hh"
#include"testing.mdl.hh"
#include<iostream>

int main ()try{
  ::cmw::BufferStack<SameFormat> buffer(udpServer("13579"));

  for(;;){
    buffer.getPacket();
    ::boost::base_collection<base> b;
    testing::give(buffer,b);
    ::std::cout<<"size is "<<b.size()<<::std::endl;
    ::std::cout<<"size of der1is "<<b.size<derived1>()<<::std::endl;
    ::std::cout<<"size of der3is "<<b.size<derived3>()<<::std::endl;
  }
  return 1;
}catch(std::exception const& ex){
  ::std::cout<<"failure: "<<ex.what()<<::std::endl;
}
