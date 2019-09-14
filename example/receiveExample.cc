//  This program receives messages sent by sendExample.
#include<cmw/Buffer.hh>
#include"messageIDs.hh"

#include<array>
#include<iostream>
#include<set>
#include<string>
#include<vector>
#include"exampleMessages.hh"

int main ()try{
  ::cmw::winStart();
  ::cmw::BufferStack<::cmw::SameFormat> buf(::cmw::udpServer("12345"));

  for(;;){
    buf.getPacket();
    auto const msgid=::cmw::give<messageID>(buf);
    ::std::cout<<"Message id: "<<static_cast<unsigned>(msgid)<<'\n';
    using namespace ::exampleMessages;
    switch(msgid){
      case messageID::id1:
      {
        ::std::vector<::int32_t> vec;
        ::std::string str;
        give(buf,vec,str);
        for(auto val:vec){::std::cout<<val<<' ';}
        ::std::cout<<'\n'<<str;
      }
      break;

      case messageID::id2:
      {
        ::std::set<::int32_t> iset;
        give(buf,iset);
        for(auto val:iset){::std::cout<<val<<' ';}
      }
      break;

      case messageID::id3:
      {
        ::std::array<std::array<float,2>,3> a;
        give(buf,a);
        for(auto subarray:a){
          for(auto val:subarray){::std::cout<<val<<' ';}
          ::std::cout<<'\n';
        }
      }
      break;

      default:
        ::std::cout<<"Unexpected message id: "<<msgid;
    }
    ::std::cout<<::std::endl;
  }
}catch(std::exception& e){
  ::std::cout<<"failure: "<<e.what()<<'\n';
}
