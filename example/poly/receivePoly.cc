#include<cmwComplex.hh>
#include<boost/poly_collection/base_collection.hpp>
#include"base.hh"
#include"testing.mdl.hh"
#include<iostream>

int main ()try{
  ::cmw::BufferStack<::cmw::SameFormat> buf(::cmw::udpServer("13579"));

  for(;;){
    buf.getPacket();
    ::boost::base_collection<base> b;
    ::testing::give(buf,b);
    ::std::cout<<"size is "<<b.size()<<'\n';
    ::std::cout<<"number of derived1 is "<<b.size<derived1>()<<'\n';
    ::std::cout<<"number of derived3 is "<<b.size<derived3>()<<::std::endl;
  }
  return 1;
}catch(::std::exception const& ex){
  ::std::cout<<"failure: "<<ex.what()<<::std::endl;
}
