#include"base.hh"
#include<cmwBuffer.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"testing.mdl.hh"
#include<iostream>

int main ()try{
  ::cmw::BufferStack<::cmw::SameFormat> buffer(::cmw::udpServer("13579"));

  for(;;){
    buffer.getPacket();
    ::boost::base_collection<base> b;
    ::testing::give(buffer,b);
    ::std::cout<<"size is "<<b.size()<<'\n';
    ::std::cout<<"number of der1 is "<<b.size<derived1>()<<'\n';
    ::std::cout<<"number of der3 is "<<b.size<derived3>()<<::std::endl;
  }
  return 1;
}catch(::std::exception const& ex){
  ::std::cout<<"failure: "<<ex.what()<<::std::endl;
}
